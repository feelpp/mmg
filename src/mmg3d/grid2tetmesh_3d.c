/* =============================================================================
**  This file is part of the mmg software package for the tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/CNRS/Inria/UBordeaux/UPMC, 2004-
**
**  mmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  mmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with mmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the mmg distribution only if you accept them.
** =============================================================================
*/

/**
 * \file grid2tetmesh_3d.c
 * \brief Conversion of a structured grid into a tetrahedral mesh.
 * \author Juliette Busquet (Enseirb-Matmeca)
 * \author Antoine Huc (Enseirb-Matmeca)
 * \author Fanny Kuhn (Enseirb-Matmeca)
 * \author Algiane Froehly (Inria)
 * \version 1
 * \copyright GNU Lesser General Public License.
 *
 * Conversion of a structured grid into a tetrahedral mesh.
 *
 */

#include "mmg3d.h"

/**
 * \param mesh pointer toward a mesh structure.
 * \param sol pointer toward a solution structure.
 *
 * \return 1 if success, 0 if fail.
 *
 * Create the initial octree from the strcuture grid implicitely loaded.
 *
 * Input data :
 * - mesh->freeint[i] is the number of cells in the i-direction
 * - mesh->info.min[i] stores the origin of the grid;
 * - mesh->info.max[i] stores the size of a cell in the i-direction
 *
 * \remark for now, we suppose that the grid is aligned with the canonical
 * directions
 */

static inline
int MMG3D_convert_grid2smallOctree(MMG5_pMesh mesh, MMG5_pSol sol) {
  MMG5_MOctree_s *po;
  double         length[3];
  int            i,ip,depth_int,depth_max,max_dim;

  /** Step 1: Allocation and initialization of the octree root */
  /* Creation of the bottom-left-front corner of the root cell (grid origin) and
   * computation of the octree length */

  max_dim=0;
  ip = 1;
  for ( i=0; i<3; ++i ) {
    length[i] = mesh->info.max[i] * (double)mesh->freeint[i];
    if(max_dim < mesh->freeint[i])
    {
      max_dim = mesh->freeint[i];
    }
  }
  /* Begin to work on the dual grid => we will have one cellule less in each
   * direction */
  max_dim--;

  /* set max dim to the next power of 2 */
  max_dim--;
  max_dim |= max_dim >> 1;
  max_dim |= max_dim >> 2;
  max_dim |= max_dim >> 4;
  max_dim |= max_dim >> 8;
  max_dim |= max_dim >> 16;
  max_dim++;

  depth_max=log(max_dim)/log(2);

  /* Computation of the octree length */
  /* Octree cell initialization */
  if ( !MMG3D_init_MOctree(mesh,&mesh->octree,ip,length,depth_max) ) return 0;
  po = mesh->octree->root;

  if(po->depth != depth_max)
  {
    po->nsons = 8;
  }
  else {
    po->nsons = 0;
    po->leaf=1;
  }

  /** Step 2: Octree subdivision until reaching the grid size */

  double dx = mesh->info.max[0];
  double dy = mesh->info.max[1];
  double dz = mesh->info.max[2];
  double max_distance = sqrt((dx/2.)*(dx/2.)+(dy/2.)*(dy/2.)+(dx/2.)*(dz/2.));

  MMG3D_split_MOctree_s (mesh, po, sol, max_distance);

  return 1;
}

/**
 * \param q pointer toward the MOctree cell
 * \param depth_max the depth maximum of the octree.
 *
 * \return 1 if success, 0 if fail.
 *
 * Balance an unbalanced octree in order that 2 adjacent cells have at most 1
 * level of depth of difference (2:1 balancing).
 *
 */
static inline
int MMG3D_balance_octree(MMG5_pMesh mesh, MMG5_MOctree_s* q, int depth_max) {

  int x = q->coordoct[0];
  int y = q->coordoct[1];
  int z = q->coordoct[2];
  int depth;
  int nb_cells;
  int father_id;
  MMG5_MOctree_s* p = mesh->octree->root;
  int neighboors_tab[18][3];

  // int case_nb = 0;
  // if((x == 0) || (x == pow(2,depth_max-1)))
  // {
  //   case_nb++;
  // }
  // if((y == 0) || (y == pow(2,depth_max-1)))
  // {
  //   case_nb++;
  // }
  // if((z == 0) || (z == pow(2,depth_max-1)))
  // {
  //   case_nb++;
  // }
  // if(x != octree_size)
  // {
  //   if(y != octree_size)
  //   {
  //     if(z != octree_size)
  //     {
  //       neighboors_tab[0][0] = x;
  //       neighboors_tab[0][1] = y+1;
  //       neighboors_tab[0][2] = z+1;
  //     }
  //   }
  // }
  //

  //CREATION DU TABLEAU DE VOISINS
  int i,j,k,tab_id;
  tab_id=0;
  for(k = z-1 ; k <= z+1 ; k++)
  {
    for(j = y-1 ; j <= y+1 ; j++)
    {
      for(i = x-1 ; i <= x+1 ; i++)
      {
        if((i!=j || i!=k || j!=k) && (i==x || j==y || k==z))
        {
          neighboors_tab[tab_id][0] = i;
          neighboors_tab[tab_id][1] = j;
          neighboors_tab[tab_id][2] = k;
          tab_id++;
        }
      }
    }
  }

  int octree_size = pow(2,depth_max-1);

  for(i = 0 ; i < 18 ; i++)
  {
    x = neighboors_tab[i][0];
    y = neighboors_tab[i][1];
    z = neighboors_tab[i][2];

    //VERIFICATION QUE LE VOISIN APPARTIENT BIEN A LA GRILLE
    if(x >= 0 && x < octree_size && y >= 0 && y < octree_size && z >= 0 && z < octree_size)
    {
      depth = 2;
      while((p->leaf != 1) && (depth <= depth_max))
      {
        nb_cells = pow(2,depth_max-depth);
        if(x < nb_cells)
        {
          if(y < nb_cells)
          {
            if(z < nb_cells)
            {
              father_id = 0;
            }
            else
            {
              father_id = 2;
              z -= nb_cells;
            }
          }
          else
          {
            if(z < nb_cells)
            {
              father_id = 4;
            }
            else
            {
              father_id = 6;
              z -= nb_cells;
            }
            y -= nb_cells;
          }
        }
        else
        {
          if(y < nb_cells)
          {
            if(z < nb_cells)
            {
              father_id = 1;
            }
            else
            {
              father_id = 3;
              z -= nb_cells;
            }
          }
          else
          {
            if(z < nb_cells)
            {
              father_id = 5;
            }
            else
            {
              father_id = 7;
              z -= nb_cells;
            }
            y -= nb_cells;
          }
          x -= nb_cells;
        }
        p = &p->sons[father_id];
        depth++;
      }
      if(q->depth - p->depth > 0)//si la différence entre les depths est déjà supérieure à 0 pas de merge
      {
        return 0;
      }
    }
  }
  return 1;
}


/**
* \param q pointer toward the MOctree cell
 * \param depth_max the depth maximum of the octree.
 *
 * \return 1 if success, 0 if fail.
 *
 * Build the coarse octree from the initial octree.
 *
 */
static inline
int MMG3D_build_coarsen_octree(MMG5_pMesh mesh, MMG5_MOctree_s* q, int depth_max) {
  int i,leaf_sum;
  leaf_sum=0;
  for(i=0; i<q->nsons; i++)
  {
    leaf_sum += q->sons[i].leaf;
  }

  if(leaf_sum != q->nsons) // si je ne suis pas un  père QUE de leafs (anciennes et nouvelles)
  {
    for(i=0; i<q->nsons; i++)
    {
      if(q->sons[i].leaf != 1)//si je ne suis pas une leaf
      {
        MMG3D_build_coarsen_octree(mesh, &q->sons[i], depth_max);
      }
    }
  }
  if(leaf_sum == q->nsons) // si je suis un père de leafs (anciennes et nouvelles)
  {
    // crée le split_ls du père
    i=0;
    while(q->sons[i].split_ls==0)
    {
      i++;
    }
    if(i!=q->nsons-1)
    {
      q->split_ls=1; // vérifie si au moins un fils possède la LS
    }

    if(q->split_ls == 0)
    {
      if(MMG3D_balance_octree(mesh,q,depth_max))
      {
        MMG3D_merge_MOctree_s (q, mesh);
      }
    }
  }
  leaf_sum=0;
  return 1;
}


/**
 * \param mesh pointer toward a mesh structure.
 * \param sol pointer toward a solution structure.
 *
 * \return 1 if success, 0 if fail.
 *
 * Create a coarse octree from the initial octree.
 *
 */
static inline
int MMG3D_coarsen_octree(MMG5_pMesh mesh, MMG5_pSol sol) {
  MMG5_MOctree_s *po;
  po=mesh->octree->root;

  int i, depth_max;
  int max_dim=0;
  for ( i=0; i<3; ++i ) {
    if(max_dim < mesh->freeint[i])
    {
      max_dim = mesh->freeint[i];
    }
  }
  // set max dim to the next power of 2
  max_dim--;
  max_dim |= max_dim >> 1;
  max_dim |= max_dim >> 2;
  max_dim |= max_dim >> 4;
  max_dim |= max_dim >> 8;
  max_dim |= max_dim >> 16;
  max_dim++;

  depth_max=log(max_dim)/log(2);

  MMG3D_build_coarsen_octree(mesh, po, depth_max);

  return 1;
}


/**
 * \param mesh pointer toward a mesh structure.
 * \param sol pointer toward a solution structure.
 *
 * \return 1 if success, 0 if fail.
 *
 * Convert a balanced octree (2:1) into a tetrahedral mesh.
 *
 */
static inline
int MMG3D_convert_octree2tetmesh(MMG5_pMesh mesh, MMG5_pSol sol) {

  printf ( " %s:%s: TO IMPLEMENT\n",__FILE__,__func__ ); return 0;

  return 1;
}

/**
 * \param mesh pointer toward a mesh structure.
 * \param sol pointer toward a solution structure that contains the solution at
 * grid centroids at the beginning and at mesh nodes at the end.
 *
 * \return 1 if success, 0 if fail.
 *
 * Convert a strcutured grid into an octree, then transform this octree into a
 * tetrahedral mesh.
 *
 */
int MMG3D_convert_grid2tetmesh(MMG5_pMesh mesh, MMG5_pSol sol) {

  /**--- stage 1: Octree computation */
  if ( abs(mesh->info.imprim) > 3 )
    fprintf(stdout,"\n  ** OCTREE INITIALIZATION\n");

  /* Conversion of the grid into an octree */
  if ( !MMG3D_convert_grid2smallOctree(mesh,sol) ) {
    fprintf(stderr,"\n  ## Octree initialization problem. Exit program.\n");
    return 0;
  }

  /* Creation of the coarse octree */
  if ( !MMG3D_coarsen_octree(mesh,sol) ) {
    fprintf(stderr,"\n  ## Octree coarsening problem. Exit program.\n");
    return 0;
  }
  //
  // /* Octree balancing */
  // if ( !MMG3D_balance_octree(mesh,sol) ) {
  //   fprintf(stderr,"\n  ## Octree balancing problem. Exit program.\n");
  //   return 0;
  // }
  //
  /**--- stage 2: Tetrahedralization */
  if ( abs(mesh->info.imprim) > 3 )
    fprintf(stdout,"\n  ** OCTREE TETRAHEDRALIZATION\n");

  if ( !MMG3D_convert_octree2tetmesh(mesh,sol) ) {
    fprintf(stderr,"\n  ## Octree tetrahedralization problem. Exit program.\n");
    return 0;
  }

  return 1;
}
