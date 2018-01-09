/* ESOP
 * Copyright (C) 2018  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(CRYPTOMINISAT_EXTENSION) && defined(KITTY_EXTENSION)

#include <esop/exact_synthesis.hpp>
#include <sat/sat_solver.hpp>

namespace esop
{

/******************************************************************************
 * Types                                                                      *
 ******************************************************************************/

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

inline bool get_bit( const kitty::cube& c, uint8_t index )
{
  return ( c._bits & ( 1 << index ) ) == ( 1 << index );
}

inline void set_mask( kitty::cube &c, uint8_t index )
{
  c._mask |= (1 << index);
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

esops_t exact_synthesis_from_binary_string( const std::string& binary, unsigned max_number_of_cubes )
{
  const int num_vars = log2( binary.size() );
  assert( binary.size() == (1ull << num_vars) && "bit-width is not a power of 2" );

  assert( num_vars <= 32 && "cube data structure cannot store more than 32 variables" );

  esop::esops_t esops;
  for ( auto k = 1u; k <= max_number_of_cubes; ++k )
  {
    std::cout << "[i] bounded synthesis for k = " << k << std::endl;

    sat::sat_solver solver;

    /* add constraints */
    kitty::cube minterm = kitty::cube::neg_cube( num_vars );

    auto sample_counter = 0u;
    do
    {
      /* skip don't cares */
      if ( binary[ minterm._bits ] != '0' && binary[ minterm._bits ] != '1' )
      {
	++minterm._bits;
	continue;
      }

      std::vector<int> xor_clause;

      for ( auto j = 0u; j < k; ++j )
      {
	const int z = 1 + 2*num_vars*k + sample_counter*k + j;
	xor_clause.push_back( z );

	// positive
	for ( auto l = 0; l < num_vars; ++l )
	{
	  if ( get_bit( minterm, l ) )
	  {
	    std::vector<int> clause;
	    clause.push_back( -z ); // - z_j
	    clause.push_back( -( 1 + num_vars*k + num_vars*j + l ) ); // -q_j,l

	    solver.add_clause( clause );
	  }
	  else
	  {
	    std::vector<int> clause;
	    clause.push_back( -z ); // -z_j
	    clause.push_back( -( 1 + num_vars*j + l ) ); // -p_j,l

	    solver.add_clause( clause );
	  }
	}
      }

      for ( auto j = 0u; j < k; ++j )
      {
	const int z = 1 + 2*num_vars*k + sample_counter*k + j;

	// negative
	std::vector<int> clause = { z };
	for ( auto l = 0; l < num_vars; ++l )
	{
	  if ( get_bit( minterm, l ) )
	  {
	    clause.push_back( 1 + num_vars*k + num_vars*j + l ); // q_j,l
	  }
	  else
	  {
	    clause.push_back( 1 + num_vars*j + l ); // p_j,l
	  }
	}

	solver.add_clause( clause );
      }

      solver.add_xor_clause( xor_clause, binary[ minterm._bits ] == '1' );

      ++sample_counter;
      ++minterm._bits;
    } while( minterm._bits < (1 << num_vars) );

    while ( auto result = solver.solve() )
    {
      esop::esop_t esop;

      std::vector<int> blocking_clause;
      for ( auto j = 0u; j < k; ++j )
      {
	kitty::cube c;
	bool cancel_cube = false;
	for ( auto l = 0; l < num_vars; ++l )
	{
	  const auto p_value = result.model[ j*num_vars + l ] == CMSat::l_True;
	  const auto q_value = result.model[ num_vars*k + j*num_vars + l ] == CMSat::l_True;

	  blocking_clause.push_back( p_value ? -(1 + j*num_vars + l) : (1 + j*num_vars + l) );
	  blocking_clause.push_back( q_value ? -(1 + num_vars*k + j*num_vars + l) : (1 + num_vars*k + j*num_vars + l) );

	  if ( p_value && q_value )
	  {
	    cancel_cube = true;
	  }
	  else if ( p_value )
	  {
	    c.add_literal( l, true );
	  }
	  else if ( q_value )
	  {
	    c.add_literal( l, false );
	  }
	}

	if ( cancel_cube )
	{
	  continue;
	}

	esop.push_back( c );
      }
      esops.push_back( esop );

      solver.add_clause( blocking_clause );
    }

    if ( esops.size() > 0 )
    {
      break;
    }
  }

  return esops;
}

} /* esop */

#endif /* CRYPTOMINISAT_EXTENSION && KITTY_EXTENSION */

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
