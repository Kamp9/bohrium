/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium team:
http://bohrium.bitbucket.org

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with Bohrium. 

If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __BOHRIUM_BRIDGE_CPP_STATE
#define __BOHRIUM_BRIDGE_CPP_STATE
#include <iostream>
#include <boost/ptr_container/ptr_map.hpp>
#include "bh.h"

namespace bh {

#define BH_CPP_QUEUE_MAX 1024
static bh_instruction queue[BH_CPP_QUEUE_MAX]; // Instruction queue
static bh_intp queue_size = 0;

bh_init      vem_init;
bh_execute   vem_execute;
bh_shutdown  vem_shutdown;

bh_reg_func vem_reg_func;
bh_component    **components,
                *self_component,
                *vem_component;
bh_intp children_count;

typedef boost::ptr_map<int, bh_array> storage_type;
storage_type    storage;

int keys = 0;

/**
 * State should be encapsulated into this "Runtime" singleton.
 *
 */
class Runtime {
public:
    static Runtime* instance();

    template <typename T>
    void enqueue( T cool );

private:
    static Runtime* pInstance;

    Runtime(); // No external instanciation.

};

Runtime* Runtime::pInstance = 0;

Runtime* Runtime::instance() {
    if (pInstance == 0) {
        pInstance = new Runtime;
    }
    return pInstance;
}

Runtime::Runtime() {
    std::cout << "Creating!" << std::endl;
}

template <typename T>
void Runtime::enqueue( T cool ) {
    std::cout << "Cool " << cool << std::endl;
}

// put this into runtime constructor
void init()
{
    bh_error err;
    self_component = bh_component_setup(NULL);
    bh_component_children( self_component, &children_count, &components );

    if(children_count != 1 || components[0]->type != BH_VEM) {

        fprintf(stderr, "Error in the configuration: the bridge must "
                        "have exactly one child of type VEM\n");
        exit(-1);
    }
    vem_component   = components[0];

    vem_init        = vem_component->init;
    vem_execute     = vem_component->execute;
    vem_shutdown    = vem_component->shutdown;

    vem_reg_func        = vem_component->reg_func;
    free(components);

    err = vem_init(vem_component);
    if(err) {
        fprintf(stderr, "Error in vem_init()\n");
        exit(-1);
    }

}

// member function of runtime
bh_intp flush()
{
    char *msg = (char*) malloc(1024 * sizeof(char));

    bh_intp cur_size = queue_size;
    bh_error res = BH_SUCCESS;
    if (queue_size > 0) {
        res = vem_execute( queue_size, queue );

        if (res != BH_SUCCESS) {
            sprintf(msg, "Error in scheduled batch of instructions: %s\n", bh_error_text(res));
            printf("%s", msg);
        }

        queue_size = 0;
    }
    return cur_size;
}

// Put this into runtime deconstructor
void shutdown()
{
    flush();
    vem_shutdown();
    bh_component_free(self_component);
    bh_component_free(vem_component);
}

// member function of runtime
template <typename T>
inline
void enqueue( bh_opcode opcode, Vector<T> & op0, Vector<T> & op1, Vector<T> & op2)
{
    Runtime::instance()->enqueue( 1 );

    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }
    
    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = &storage[op2.getKey()];

    if (op1.isTemp()) {
        instr = &queue[queue_size++];
        instr->opcode = BH_FREE;
        instr->operand[0] = &storage[op1.getKey()];

        instr = &queue[queue_size++];
        instr->opcode = BH_DISCARD;
        instr->operand[0] = &storage[op1.getKey()];
    }

    if (op2.isTemp()) {
        instr = &queue[queue_size++];
        instr->opcode = BH_FREE;
        instr->operand[0] = &storage[op2.getKey()];

        instr = &queue[queue_size++];
        instr->opcode = BH_DISCARD;
        instr->operand[0] = &storage[op2.getKey()];
    }

}

template <typename T>
inline
void enqueue( bh_opcode opcode, Vector<T> & op0, Vector<T> & op1, T const& op2)
{
    Runtime::instance()->enqueue( 2 );

    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = NULL;
    assign_const_type( &instr->constant, op2 );


    if (op1.isTemp()) {
        instr = &queue[queue_size++];
        instr->opcode = BH_FREE;
        instr->operand[0] = &storage[op1.getKey()];

        instr = &queue[queue_size++];
        instr->opcode = BH_DISCARD;
        instr->operand[0] = &storage[op1.getKey()];
    }

}

template <typename T>
inline
void enqueue( bh_opcode opcode, Vector<T> & op0, T const& op1, Vector<T> & op2)
{
    Runtime::instance()->enqueue( 3 );

    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = NULL;
    instr->operand[2] = &storage[op2.getKey()];
    assign_const_type( &instr->constant, op1 );


    if (op2.isTemp()) {
        instr = &queue[queue_size++];
        instr->opcode = BH_FREE;
        instr->operand[0] = &storage[op2.getKey()];

        instr = &queue[queue_size++];
        instr->opcode = BH_DISCARD;
        instr->operand[0] = &storage[op2.getKey()];
    }

}

template <typename T>
inline
void enqueue( bh_opcode opcode, Vector<T> & op0, Vector<T> & op1)
{
    Runtime::instance()->enqueue( 4 );

    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = NULL;

    if (op1.isTemp()) {
        instr = &queue[queue_size++];
        instr->opcode = BH_FREE;
        instr->operand[0] = &storage[op1.getKey()];

        instr = &queue[queue_size++];
        instr->opcode = BH_DISCARD;
        instr->operand[0] = &storage[op1.getKey()];
    }

}

template <typename T>
inline
void enqueue( bh_opcode opcode, Vector<T> & op0, T const& op1)
{
    Runtime::instance()->enqueue( 5 );

    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = NULL;
    instr->operand[2] = NULL;
    assign_const_type( &instr->constant, op1 );
}

}

#endif
