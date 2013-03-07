/*
This file is part of Bohrium and Copyright (c) 2012 the Bohrium team:
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
GNU Lesser General Public License along with bohrium. 

If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include "bh/cppb.hpp"

using namespace bh;

void compute()
{
    bool first = true;

    Vector<double> x = Vector<double>(3, 3);
    Vector<double> y = Vector<double>(3, 3);
    x = 3.0;

    std::cout << "Priting values: ";
    for(Vector<double>::iterator it=x.begin(); it != x.end(); it++) {

        if (!first) {
            std::cout  << ", ";
        } else {
            first = false;
        }
        std::cout << *it;
    }

    std::cout << "." << std::endl;

}

int main()
{
    init();
    compute();
    shutdown();

    return 0;
}
