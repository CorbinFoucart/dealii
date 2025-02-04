// ---------------------------------------------------------------------
//
// Copyright (C) 2018 - 2020 by the deal.II authors
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

#include <deal.II/base/function_parser.h>
#include <deal.II/base/point.h>
#include <deal.II/base/tensor.h>

#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>

#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/sparse_direct.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/sparsity_pattern.h>

// #include "/workspace/dealii/dealii-0/source/non_matching/coupling.cc"
#include <deal.II/non_matching/coupling.h>

#include <deal.II/numerics/matrix_tools.h>

#include "../tests.h"

// Test that a coupling matrix can be constructed for each pair of dimension and
// immersed dimension, and check that constants are projected correctly
// even when locally refined grids are used.

template <int dim0, int dim1, int spacedim>
void
test()
{
  static_assert(dim0 >= dim1, "Only dim0>=dim1 is possible");

  deallog << "dim0: " << dim0 << ", dim1: " << dim1
          << ", spacedim: " << spacedim << std::endl;

  Triangulation<dim0, spacedim> tria0;
  Triangulation<dim1, spacedim> tria1;

  GridGenerator::hyper_cube(tria0, -1, 1);
  GridGenerator::hyper_cube(tria1, -.44444, .3333);

  tria0.refine_global((dim0 < 3 ? 3 : 2));
  tria1.refine_global(2);

  for (unsigned int i = 0; i < 2; ++i)
    {
      for (const auto &cell : tria1.active_cell_iterators())
        if (cell->center()[0] < 0)
          cell->set_refine_flag();
      tria1.execute_coarsening_and_refinement();

      for (const auto &cell : tria0.active_cell_iterators())
        if (cell->center()[0] > 0)
          cell->set_refine_flag();
      tria0.execute_coarsening_and_refinement();
    }

  FE_Q<dim0, spacedim> fe0(1);
  FE_Q<dim1, spacedim> fe1(1);

  DoFHandler<dim0, spacedim> dh0(tria0);
  DoFHandler<dim1, spacedim> dh1(tria1);

  GridTools::Cache<dim0, spacedim> cache0(tria0);
  GridTools::Cache<dim1, spacedim> cache1(tria1);

  dh0.distribute_dofs(fe0);
  dh1.distribute_dofs(fe1);

  AffineConstraints<double> constraints0;
  AffineConstraints<double> constraints1;

  DoFTools::make_hanging_node_constraints(dh0, constraints0);
  DoFTools::make_zero_boundary_constraints(dh0, constraints0);

  DoFTools::make_hanging_node_constraints(dh1, constraints1);

  constraints0.close();
  constraints1.close();

  deallog << "FE0                : " << fe0.get_name() << std::endl
          << "FE1                : " << fe1.get_name() << std::endl
          << "Dofs 0             : " << dh0.n_dofs() << std::endl
          << "Dofs 1             : " << dh1.n_dofs() << std::endl
          << "Constrained dofs 0 : " << constraints0.n_constraints()
          << std::endl
          << "Constrained dofs 1 : " << constraints1.n_constraints()
          << std::endl;

  QGauss<dim0> quad0(2); // Quadrature for coupling
  QGauss<dim1> quad1(2); // Quadrature for coupling

  SparsityPattern sparsity;
  {
    DynamicSparsityPattern dsp(dh0.n_dofs(), dh1.n_dofs());
    NonMatching::create_coupling_sparsity_pattern(
      cache0,
      dh0,
      dh1,
      quad1,
      dsp,
      constraints0,
      ComponentMask(),
      ComponentMask(),
      StaticMappingQ1<dim1, spacedim>::mapping,
      constraints1);
    sparsity.copy_from(dsp);
  }
  SparseMatrix<double> coupling(sparsity);

  NonMatching::create_coupling_mass_matrix(
    cache0,
    dh0,
    dh1,
    quad1,
    coupling,
    constraints0,
    ComponentMask(),
    ComponentMask(),
    StaticMappingQ1<dim1, spacedim>::mapping,
    constraints1);

  SparsityPattern mass_sparsity1;
  {
    DynamicSparsityPattern dsp(dh1.n_dofs(), dh1.n_dofs());
    DoFTools::make_sparsity_pattern(dh1, dsp, constraints1, false);
    mass_sparsity1.copy_from(dsp);
  }
  SparseMatrix<double> mass_matrix1(mass_sparsity1);
  MatrixTools::create_mass_matrix(dh1,
                                  quad1,
                                  mass_matrix1,
                                  static_cast<const Function<spacedim> *>(
                                    nullptr),
                                  constraints1);

  SparseDirectUMFPACK mass_matrix1_inv;
  mass_matrix1_inv.factorize(mass_matrix1);

  // now take ones in dh0, project them onto dh1,
  // get back ones, and check for the error.
  //
  // WARNINGS: Only works if dh1 is immersed in dh0

  Vector<double> ones0(dh0.n_dofs());
  Vector<double> ones1(dh1.n_dofs());

  ones0 = 1.0;
  constraints0.distribute(ones0);

  coupling.Tvmult(ones1, ones0);
  mass_matrix1_inv.solve(ones1);
  constraints1.distribute(ones1);

  Vector<double> exact_ones1(dh1.n_dofs());
  exact_ones1 = 1.0;
  constraints1.distribute(exact_ones1);
  ones1 -= exact_ones1;

  deallog << "Error on constants: " << ones1.l2_norm() << std::endl;
}

int
main()
{
  initlog(true);
  test<1, 1, 1>();
  test<2, 1, 2>();
  test<2, 2, 2>();
  test<3, 2, 3>();
  test<3, 3, 3>();
}
