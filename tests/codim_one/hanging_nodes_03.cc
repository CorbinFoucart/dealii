// ---------------------------------------------------------------------
//
// Copyright (C) 2010 - 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------



// a further extract of the _02 test. the results here are correct and
// show the relationship between the various cells of the mesh

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria.h>

#include "../tests.h"


int
main()
{
  initlog();

  const unsigned int spacedim = 3;
  const unsigned int dim      = spacedim - 1;

  Triangulation<dim, spacedim> boundary_mesh;

  /*****************************************************************/
  // Create Surface Mesh:  Boundary of hypercube without one face
  {
    Triangulation<spacedim> volume_mesh;
    GridGenerator::hyper_cube(volume_mesh);
    Triangulation<spacedim>::active_cell_iterator cell =
      volume_mesh.begin_active();

    cell->face(0)->set_all_boundary_ids(1);
    std::set<types::boundary_id> boundary_ids;
    boundary_ids.insert(0);
    GridGenerator::extract_boundary_mesh(volume_mesh,
                                         boundary_mesh,
                                         boundary_ids);
  }

  Triangulation<dim, spacedim>::active_cell_iterator cell =
    boundary_mesh.begin_active();
  for (; cell != boundary_mesh.end(); ++cell)
    {
      deallog << "Cell = " << cell << std::endl;
      deallog << "  direction_flag = " << cell->direction_flag() << std::endl;

      for (const unsigned int face : GeometryInfo<dim>::face_indices())
        {
          deallog << "  face = " << face
                  << "  (neighbor = " << cell->neighbor(face) << ')'
                  << std::endl;

          if (cell->face(face)->has_children())
            for (unsigned int c = 0; c < cell->face(face)->n_children(); ++c)
              {
                deallog << "    subface = " << c << std::endl;
              }
        }
    }

  return 0;
}
