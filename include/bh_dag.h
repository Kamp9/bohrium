/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

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

#ifndef __BH_IR_DAG_H
#define __BH_IR_DAG_H

#include <boost/foreach.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/graphviz.hpp>
#include <iterator>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <bh.h>

namespace bohrium {
namespace dag {

struct EdgeWeight
{
    int64_t value;
    EdgeWeight():value(0){}
    EdgeWeight(int64_t weight):value(weight){}
};

typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS,
                              bh_ir_kernel> GraphD;
typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS,
                              boost::no_property, EdgeWeight> GraphW;

typedef uint64_t Vertex;
typedef typename boost::graph_traits<GraphD>::edge_descriptor EdgeD;
typedef typename boost::graph_traits<GraphW>::edge_descriptor EdgeW;


/* Determines whether there exist a path from 'a' to 'b' with
 * length more than one ('a' and 'b' is not adjacent).
 *
 * Complexity: O(E + V)
 *
 * @a       The first vertex
 * @b       The second vertex
 * @dag     The DAG
 * @return  True if there is a long path, else false
 */
bool path_exist(const Vertex &a, const Vertex &b, const GraphD &dag)
{
    using namespace std;
    using namespace boost;

    struct local_visitor:default_bfs_visitor
    {
        const Vertex dst;
        local_visitor(const Vertex &b):dst(b){};

        void examine_edge(EdgeD e, const GraphD &g) const
        {
            if(source(e,g) == dst or target(e,g) == dst)
                throw runtime_error("");
        }
    };
    try
    {
        breadth_first_search(dag, a, visitor(local_visitor(b)));
    }
    catch (const runtime_error &e)
    {
        return true;
    }
    return false;
}


class GraphDW
{
    GraphD _bglD;
    GraphW _bglW;

public:
    const GraphD &bglD() const {return _bglD;}
    const GraphW &bglW() const {return _bglW;}

    EdgeD add_edgeD(Vertex a, Vertex b)
    {
        return boost::add_edge(a, b, _bglD).first;
    }
    EdgeW add_edgeW(Vertex a, Vertex b, int64_t weight=-1)
    {
        return boost::add_edge(a, b, EdgeWeight(weight), _bglW).first;
    }
    Vertex add_vertex(const bh_ir_kernel &kernel)
    {
        Vertex d = boost::add_vertex(kernel, _bglD);
        Vertex w = boost::add_vertex(_bglW);
        assert(w == d);

        //Add edges
        BOOST_REVERSE_FOREACH(Vertex v, vertices(_bglD))
        {
            if(d != v and not path_exist(v, d, _bglD))
            {
                bool dependency = false;
                if(kernel.dependency(_bglD[v]))
                {
                    dependency = true;
                    add_edge(v, d, _bglD);
                }
                int64_t cost = kernel.dependency_cost(_bglD[v]);
                if((cost > 0) or (cost == 0 and dependency))
                {
                    add_edgeW(v, d, cost);
                }
            }
        }
        return d;
    }
    void add_instr(Vertex v, const bh_instruction &i)
    {
        _bglD[v].add_instr(i);
    }
    void clear_vertex(Vertex v)
    {
        boost::clear_vertex(v, _bglD);
        boost::clear_vertex(v, _bglW);
        _bglD[v] = bh_ir_kernel();
    }
    void remove_vertex(Vertex v)
    {
        boost::remove_vertex(v, _bglD);
        boost::remove_vertex(v, _bglW);
    }
    void remove_edgeD(EdgeD a)
    {
        boost::remove_edge(a, _bglD);
    }
    void update_weights(Vertex v)
    {
        std::vector<EdgeW> removes;
        BOOST_FOREACH(const EdgeW &e, out_edges(v, _bglW))
        {
            Vertex src = source(e, _bglW);
            Vertex dst = target(e, _bglW);
            int64_t cost = _bglD[dst].dependency_cost(_bglD[src]);
            if(cost >= 0)
            {
                _bglW[e].value = cost;
            }
            else
            {
                removes.push_back(e);
            }
        }
        BOOST_FOREACH(const EdgeW &e, removes)
        {
            boost::remove_edge(e, _bglW);
        }
    }
    void remove_empty_vertices()
    {
        std::vector<Vertex> removes;
        BOOST_FOREACH(Vertex v, vertices(_bglD))
        {
            if(_bglD[v].instr_list().size() == 0)
            {
                removes.push_back(v);
            }
        }
        //NB: because of Vertex invalidation, we have to traverse in reverse
        BOOST_REVERSE_FOREACH(Vertex &v, removes)
        {
            remove_vertex(v);
        }
    }
};

/* Creates a new DAG based on a bhir that consist of single
 * instruction kernels.
 * NB: a vertex in the 'dag' must bundle with the bh_ir_kernel class
 *
 * Complexity: O(n^2) where 'n' is the number of instructions
 *
 * @bhir  The BhIR
 * @dag   The output dag
 *
 * Throw logic_error() if the kernel_list wihtin 'bhir' isn't empty
 */
void from_bhir(const bh_ir &bhir, GraphDW &dag)
{
    using namespace std;
    using namespace boost;

    if(bhir.kernel_list.size() != 0)
    {
        throw logic_error("The kernel_list is not empty!");
    }
    //Build a singleton DAG
    BOOST_FOREACH(const bh_instruction &instr, bhir.instr_list)
    {
        bh_ir_kernel k;
        k.add_instr(instr);
        dag.add_vertex(k);
    }
}

/* Creates a new DAG based on a kernel list where each vertex is a kernel.
 *
 * Complexity: O(E + V)
 *
 * @kernels The kernel list
 * @dag     The output dag
 */
void from_kernels(const std::vector<bh_ir_kernel> &kernels, GraphDW &dag)
{
    using namespace std;
    using namespace boost;

    BOOST_FOREACH(const bh_ir_kernel &kernel, kernels)
    {
        if(kernel.instr_list().size() == 0)
            continue;

        dag.add_vertex(kernel);
    }
}

/* Fills the kernel list based on the DAG where each vertex is a kernel.
 *
 * Complexity: O(E + V)
 *
 * @dag     The dag
 * @kernels The kernel list output
 */
void fill_kernels(const GraphD &dag, std::vector<bh_ir_kernel> &kernels)
{
    using namespace std;
    using namespace boost;

    vector<Vertex> topological_order;
    topological_sort(dag, back_inserter(topological_order));
    BOOST_REVERSE_FOREACH(const Vertex &v, topological_order)
    {
        if(dag[v].instr_list().size() > 0)
            kernels.push_back(dag[v]);
    }
}

/* Determines whether there are cycles in the Graph
 *
 * Complexity: O(E + V)
 *
 * @g       The digraph
 * @return  True if there are cycles in the digraph, else false
 */
bool cycles(const GraphD &g)
{
    using namespace std;
    using namespace boost;
    try
    {
        //TODO: topological sort is an efficient method for finding cycles,
        //but we should avoid allocating a vector
        vector<Vertex> topological_order;
        topological_sort(g, back_inserter(topological_order));
        return false;
    }
    catch (const not_a_dag &e)
    {
        return true;
    }
}

/* Clear the vertex without actually removing it.
 * NB: invalidates all existing edge iterators
 *     but NOT pointers to neither vertices nor edges.
 *
 * Complexity: O(1)
 *
 * @dag  The DAG
 * @v    The Vertex
 */
void nullify_vertex(GraphDW &dag, const Vertex &v)
{
    dag.clear_vertex(v);
}

/* Merge vertex 'a' and 'b' by appending 'b's instructions to 'a'.
 * Vertex 'b' is nullified rather than removed thus existing vertex
 * and edge pointers are still valid after the merge.
 *
 * NB: invalidates all existing edge iterators.
 *
 * Complexity: O(1)
 *
 * @a   The first vertex
 * @b   The second vertex
 * @dag The DAG
 */
void merge_vertices(const Vertex &a, const Vertex &b, GraphDW &dag)
{
    using namespace std;
    using namespace boost;

    vector<pair<Vertex, Vertex> > edges2add;
    BOOST_FOREACH(const bh_instruction &i, dag.bglD()[b].instr_list())
    {
        dag.add_instr(a, i);
    }
    BOOST_FOREACH(const Vertex &v, adjacent_vertices(b, dag.bglD()))
    {
        if(a != v)
            edges2add.push_back(make_pair(a, v));
    }
    BOOST_FOREACH(const Vertex &v, inv_adjacent_vertices(b, dag.bglD()))
    {
        if(a != v)
            edges2add.push_back(make_pair(v, a));
    }
    pair<Vertex,Vertex> e;
    BOOST_FOREACH(e, edges2add)
    {
        dag.add_edgeW(e.first, e.second);
        dag.add_edgeD(e.first, e.second);
    }
    nullify_vertex(dag, b);
    dag.update_weights(a);
}

/* Merge the vertices specified by a list of edges and write
 * the result to new_dag, which should be empty.
 *
 * Complexity: O(V + E)
 *
 * @dag          The input DAG
 * @edges2merge  List of weight edges that specifies which
 *               pair of vertices to merge
 * @new_dag      The output DAG
 * @return       Whether all merges where fusible
 */
bool merge_vertices(GraphDW &dag, const std::vector<EdgeW> edges2merge)
{
    using namespace std;
    using namespace boost;

    //Help function to find the new location
    struct find_new_location
    {
        Vertex operator()(map<Vertex, Vertex> &loc_map, Vertex v)
        {
            Vertex v_mapped = loc_map[v];
            if(v_mapped == v)
                return v;
            else
                return (*this)(loc_map, v_mapped);
        }
    }find_loc;
    bool fusibility = true;

    //'loc_map' maps a vertex before the merge to the corresponding vertex after the merge
    map<Vertex, Vertex> loc_map;
    BOOST_FOREACH(const Vertex &v, vertices(dag.bglD()))
    {
        loc_map[v] = v;
    }

    BOOST_FOREACH(const EdgeW &e, edges2merge)
    {
        Vertex v1 = find_loc(loc_map, target(e, dag.bglW()));
        Vertex v2 = find_loc(loc_map, source(e, dag.bglW()));
        if(v1 != v2)
        {
            if(not dag.bglD()[v1].fusible(dag.bglD()[v2]))
                fusibility = false;
            merge_vertices(v1, v2, dag);
            loc_map[v2] = v1;
        }
    }
    return fusibility;
}

/* Determines the cost of the DAG.
 *
 * Complexity: O(E + V)
 *
 * @dag     The DAG
 * @return  The cost
 */
uint64_t dag_cost(const GraphD &dag)
{
    using namespace std;
    using namespace boost;

    uint64_t cost = 0;
    BOOST_FOREACH(const Vertex &v, vertices(dag))
    {
        cost += dag[v].cost();
    }
    return cost;
}

/* Sort the weights in descending order
 *
 * Complexity: O(E * log E)
 *
 * @edges  The input/output edge list
 */
void sort_weights(const GraphW &dag, std::vector<EdgeW> &edges)
{
    struct wcmp
    {
        const GraphW &graph;
        wcmp(const GraphW &d): graph(d){}
        bool operator() (const EdgeW &e1, const EdgeW &e2)
        {
            return (graph[e1].value > graph[e2].value);
        }
    };
    sort(edges.begin(), edges.end(), wcmp(dag));
}

/* Writes the DOT file of a DAG
 *
 * Complexity: O(E + V)
 *
 * @dag       The DAG to write
 * @filename  The name of DOT file
 */
void pprint(const GraphDW &dag, const char filename[])
{
    using namespace std;
    using namespace boost;
    //Lets create a graph with both vertical and horizontal edges
    GraphD new_dag(dag.bglD());
    map<pair<Vertex, Vertex>, pair<int64_t, bool> > weights;

    BOOST_FOREACH(const EdgeW &e, edges(dag.bglW()))
    {
        Vertex src = source(e, dag.bglW());
        Vertex dst = target(e, dag.bglW());
        bool exist = edge(src,dst,new_dag).second or edge(dst,src,new_dag).second;
        if(not exist)
            add_edge(src, dst, new_dag);

        //Save an edge map of weights and if it is directed
        weights[make_pair(src,dst)] = make_pair(dag.bglW()[e].value, exist);
    }

    //We define a graph and a kernel writer for graphviz
    struct graph_writer
    {
        const GraphD &graph;
        graph_writer(const GraphD &g) : graph(g) {};
        void operator()(std::ostream& out) const
        {
            out << "labelloc=\"t\";" << endl;
            out << "label=\"DAG with a total cost of " << dag_cost(graph);
            out << " bytes\";" << endl;
            out << "graph [bgcolor=white, fontname=\"Courier New\"]" << endl;
            out << "node [shape=box color=black, fontname=\"Courier New\"]" << endl;
        }
    };
    struct kernel_writer
    {
        const GraphD &graph;
        kernel_writer(const GraphD &g) : graph(g) {};
        void operator()(std::ostream& out, const Vertex& v) const
        {
            char buf[1024*10];
            out << "[label=\"Kernel " << v << ", cost: " << graph[v].cost();
            out << " bytes\\n";
            out << "Input views: \\l";
            BOOST_FOREACH(const bh_view &i, graph[v].input_list())
            {
                bh_sprint_view(&i, buf);
                out << buf << "\\l";
            }
            out << "Output views: \\l";
            BOOST_FOREACH(const bh_view &i, graph[v].output_list())
            {
                bh_sprint_view(&i, buf);
                out << buf << "\\l";
            }
            out << "Temp base-arrays: \\l";
            BOOST_FOREACH(const bh_base *i, graph[v].temp_list())
            {
                bh_sprint_base(i, buf);
                out << buf << "\\l";
            }
            out << "Instruction list: \\l";
            BOOST_FOREACH(const bh_instruction &i, graph[v].instr_list())
            {
                bh_sprint_instr(&i, buf, "\\l");
                out << buf << "\\l";
            }
            out << "\"]";
        }
    };
    struct edge_writer
    {
        const GraphD &graph;
        const map<pair<Vertex, Vertex>, pair<int64_t, bool> > &wmap;
        edge_writer(const GraphD &g, const map<pair<Vertex, Vertex>, pair<int64_t, bool> > &w) : graph(g), wmap(w) {};
        void operator()(std::ostream& out, const EdgeD& e) const
        {
            Vertex src = source(e, graph);
            Vertex dst = target(e, graph);
            int64_t c = -1;
            bool directed = true;
            map<pair<Vertex, Vertex>, pair<int64_t, bool> >::const_iterator it = wmap.find(make_pair(src,dst));
            if(it != wmap.end())
                tie(c,directed) = (*it).second;

            out << "[label=\" ";
            if(c == -1)
                out << "N/A\" color=red";
            else
                out << c << " bytes\"";
            if(not directed)
                out << " dir=none color=green constraint=false";
            out << "]";
        }
    };
    ofstream file;
    file.open(filename);
    write_graphviz(file, new_dag, kernel_writer(new_dag),
                   edge_writer(new_dag, weights), graph_writer(new_dag));
    file.close();
}

/* Fuse vertices in the graph that can be fused without
 * changing any future possible fusings
 * NB: invalidates all existing vertex and edge pointers.
 *
 * Complexity: O(E * (E + V))
 *
 * @dag The DAG to fuse
 */
void fuse_gentle(GraphDW &dag)
{
    using namespace std;
    using namespace boost;

    const GraphD &d = dag.bglD();
    bool not_finished = true;
    while(not_finished)
    {
        not_finished = false;
        BOOST_FOREACH(const EdgeD &e, edges(d))
        {
            const Vertex &src = source(e, d);
            const Vertex &dst = target(e, d);
            if((in_degree(dst, d) == 1 and out_degree(dst, d) == 0) or
               (in_degree(src, d) == 0 and out_degree(src, d) == 1) or
               (in_degree(dst, d) <= 1 and out_degree(src, d) <= 1))
            {
                if(d[dst].fusible_gently(d[src]))
                {
                    merge_vertices(src, dst, dag);
                    not_finished = true;
                    break;
                }
            }
        }
    }
    dag.remove_empty_vertices();
}

/* Fuse vertices in the graph greedily, which is a non-optimal
 * algorithm that fuses the most costly edges in the DAG first.
 * NB: invalidates all existing edge iterators.
 *
 * Complexity: O(E^2 * (E + V))
 *
 * @dag The DAG to fuse
 */
void fuse_greedy(GraphDW &dag)
{
    using namespace std;
    using namespace boost;

    bool not_finished = true;
    vector<EdgeW> sorted;
    while(not_finished)
    {
        not_finished = false;
        sorted.clear();
        BOOST_FOREACH(const EdgeW &e, edges(dag.bglW()))
        {
            sorted.push_back(e);
        }
        sort_weights(dag.bglW(), sorted);
        BOOST_FOREACH(const EdgeW &e, sorted)
        {
            GraphDW new_dag(dag);
            merge_vertices(source(e, dag.bglW()), target(e, dag.bglW()), new_dag);
            if(not cycles(new_dag.bglD()))
            {
                dag = new_dag;
                break;
            }
        }
    }
}

}} //namespace bohrium::dag

#endif

