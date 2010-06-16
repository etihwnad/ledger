/*
 * Copyright (c) 2003-2010, John Wiegley.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of New Artisans LLC nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <system.hh>

#include "balance.h"
#include "commodity.h"
#include "annotate.h"
#include "pool.h"
#include "unistring.h"          // for justify()

namespace ledger {

balance_t::balance_t(const double val)
{
  TRACE_CTOR(balance_t, "const double");
  amounts.insert
    (amounts_map::value_type(commodity_pool_t::current_pool->null_commodity, val));
}

balance_t::balance_t(const unsigned long val)
{
  TRACE_CTOR(balance_t, "const unsigned long");
  amounts.insert
    (amounts_map::value_type(commodity_pool_t::current_pool->null_commodity, val));
}

balance_t::balance_t(const long val)
{
  TRACE_CTOR(balance_t, "const long");
  amounts.insert
    (amounts_map::value_type(commodity_pool_t::current_pool->null_commodity, val));
}

balance_t& balance_t::operator+=(const balance_t& bal)
{
  foreach_const (const amounts_map::value_type& pair, bal.amounts,
                 amounts_map) {
    *this += pair.second;
  } foreach_end ();
  return *this;
}

balance_t& balance_t::operator+=(const amount_t& amt)
{
  if (amt.is_null())
    throw_(balance_error,
           _("Cannot add an uninitialized amount to a balance"));

  if (amt.is_realzero())
    return *this;

  amounts_map::iterator i = amounts.find(&amt.commodity());
  if (i != amounts.end())
    i->second += amt;
  else
    amounts.insert(amounts_map::value_type(&amt.commodity(), amt));

  return *this;
}

balance_t& balance_t::operator-=(const balance_t& bal)
{
  foreach_const (const amounts_map::value_type& pair, bal.amounts,
                 amounts_map) {
    *this -= pair.second;
  } foreach_end ();
  return *this;
}

balance_t& balance_t::operator-=(const amount_t& amt)
{
  if (amt.is_null())
    throw_(balance_error,
           _("Cannot subtract an uninitialized amount from a balance"));

  if (amt.is_realzero())
    return *this;

  amounts_map::iterator i = amounts.find(&amt.commodity());
  if (i != amounts.end()) {
    i->second -= amt;
    if (i->second.is_realzero())
      amounts.erase(i);
  } else {
    amounts.insert(amounts_map::value_type(&amt.commodity(), amt.negated()));
  }
  return *this;
}

balance_t& balance_t::operator*=(const amount_t& amt)
{
  if (amt.is_null())
    throw_(balance_error,
           _("Cannot multiply a balance by an uninitialized amount"));

  if (is_realzero()) {
    ;
  }
  else if (amt.is_realzero()) {
    *this = amt;
  }
  else if (! amt.commodity()) {
    // Multiplying by an amount with no commodity causes all the
    // component amounts to be increased by the same factor.
    foreach (amounts_map::value_type& pair, amounts, amounts_map) {
      pair.second *= amt;
    } foreach_end ();
  }
  else if (amounts.size() == 1) {
    // Multiplying by a commoditized amount is only valid if the sole
    // commodity in the balance is of the same kind as the amount's
    // commodity.
    if (*amounts.begin()->first == amt.commodity())
      amounts.begin()->second *= amt;
    else
      throw_(balance_error,
             _("Cannot multiply a balance with annotated commodities by a commoditized amount"));
  }
  else {
    assert(amounts.size() > 1);
    throw_(balance_error,
           _("Cannot multiply a multi-commodity balance by a commoditized amount"));
  }
  return *this;
}

balance_t& balance_t::operator/=(const amount_t& amt)
{
  if (amt.is_null())
    throw_(balance_error,
           _("Cannot divide a balance by an uninitialized amount"));

  if (is_realzero()) {
    ;
  }
  else if (amt.is_realzero()) {
    throw_(balance_error, _("Divide by zero"));
  }
  else if (! amt.commodity()) {
    // Dividing by an amount with no commodity causes all the
    // component amounts to be divided by the same factor.
    foreach (amounts_map::value_type& pair, amounts, amounts_map) {
      pair.second /= amt;
    } foreach_end ();
  }
  else if (amounts.size() == 1) {
    // Dividing by a commoditized amount is only valid if the sole
    // commodity in the balance is of the same kind as the amount's
    // commodity.
    if (*amounts.begin()->first == amt.commodity())
      amounts.begin()->second /= amt;
    else
      throw_(balance_error,
             _("Cannot divide a balance with annotated commodities by a commoditized amount"));
  }
  else {
    assert(amounts.size() > 1);
    throw_(balance_error,
           _("Cannot divide a multi-commodity balance by a commoditized amount"));
  }
  return *this;
}

optional<balance_t>
balance_t::value(const optional<datetime_t>&   moment,
                 const optional<commodity_t&>& in_terms_of) const
{
  balance_t temp;
  bool      resolved = false;

  foreach_const (const amounts_map::value_type& pair, amounts, amounts_map) {
    if (optional<amount_t> val = pair.second.value(moment, in_terms_of)) {
      temp += *val;
      resolved = true;
    } else {
      temp += pair.second;
    }
  } foreach_end ();
  
  return resolved ? temp : optional<balance_t>();
}

balance_t balance_t::price() const
{
  balance_t temp;

  foreach_const (const amounts_map::value_type& pair, amounts, amounts_map) {
    temp += pair.second.price();
  } foreach_end ();

  return temp;
}

optional<amount_t>
balance_t::commodity_amount(const optional<const commodity_t&>& commodity) const
{
  if (! commodity) {
    if (amounts.size() == 1) {
      return amounts.begin()->second;
    }
    else if (amounts.size() > 1) {
      // Try stripping annotations before giving an error.
      balance_t temp(strip_annotations(keep_details_t()));
      if (temp.amounts.size() == 1)
        return temp.commodity_amount(commodity);

      throw_(amount_error,
             _("Requested amount of a balance with multiple commodities: %1")
             << temp);
    }
  }
  else if (amounts.size() > 0) {
    amounts_map::const_iterator i =
      amounts.find(const_cast<commodity_t *>(&*commodity));
    if (i != amounts.end())
      return i->second;
  }
  return none;
}

balance_t
balance_t::strip_annotations(const keep_details_t& what_to_keep) const
{
  balance_t temp;

  foreach_const (const amounts_map::value_type& pair, amounts, amounts_map) {
    temp += pair.second.strip_annotations(what_to_keep);
  } foreach_end ();

  return temp;
}

void balance_t::print(std::ostream&       out,
                      const int           first_width,
                      const int           latter_width,
                      const uint_least8_t flags) const
{
  bool first  = true;
  int  lwidth = latter_width;

  if (lwidth == -1)
    lwidth = first_width;

  typedef std::vector<const amount_t *> amounts_array;
  amounts_array sorted;

  foreach_const (const amounts_map::value_type& pair, amounts, amounts_map) {
    if (pair.second)
      sorted.push_back(&pair.second);
  } foreach_end ();

  std::stable_sort(sorted.begin(), sorted.end(),
                   commodity_t::compare_by_commodity());

  foreach_const (const amount_t * amount, sorted, amounts_array) {
    int width;
    if (! first) {
      out << std::endl;
      width = lwidth;
    } else {
      first = false;
      width = first_width;
    }

    std::ostringstream buf;
    amount->print(buf, flags);
    justify(out, buf.str(), width, flags & AMOUNT_PRINT_RIGHT_JUSTIFY,
            flags & AMOUNT_PRINT_COLORIZE && amount->sign() < 0);
  } foreach_end ();

  if (first) {
    out.width(first_width);
    if (flags & AMOUNT_PRINT_RIGHT_JUSTIFY)
      out << std::right;
    else
      out << std::left;
    out << 0;
  }
}

void to_xml(std::ostream& out, const balance_t& bal)
{
  push_xml x(out, "balance");

  foreach_const (const balance_t::amounts_map::value_type& pair, bal.amounts,
                 balance_t::amounts_map) {
    to_xml(out, pair.second);
  } foreach_end();
}

} // namespace ledger
