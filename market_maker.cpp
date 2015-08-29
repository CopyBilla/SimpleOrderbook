/*
Copyright (C) 2015 Jonathon Ogden     < jeog.dev@gmail.com >

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include "market_maker.hpp"
#include "simple_orderbook.hpp"
#include "types.hpp"
#include <chrono>

namespace NativeLayer{

using namespace std::placeholders;

market_makers_type operator+(market_makers_type& l, market_makers_type& r)
{
  market_makers_type mms;
  for( auto& m : l)
    mms.push_back( std::move(m));
  for( auto& m: r)
    mms.push_back( std::move(m));
  l.clear();
  r.clear();
  return mms;
}

MarketMaker::MarketMaker(callback_type callback)
  :
    _book(nullptr),
    _callback( [=](callback_msg a, id_type b, price_type c, size_type d){
                  this->_base_callback(a,b,c,d);
                  callback(a,b,c,d);
                } ),
    _is_running(false),
    tick(0)
  {
  }

MarketMaker::MarketMaker()
  :
    _book(nullptr),
    _callback(std::bind(&MarketMaker::_base_callback,this,_1,_2,_3,_4)),
    _is_running(false),
    tick(0)
  {
  }

void MarketMaker::start(SimpleOrderbook::LimitInterface *book,
                        price_type implied,
                        price_type incr)
{
  if(!book)
    throw std::invalid_argument("book can not be null(ptr)");

  this->_is_running = true;
  this->_book = book;
  this->tick = incr;
}

void MarketMaker::stop()
{
  this->_is_running = false;
  this->_book = nullptr;
}

template<bool BuyNotSell>
void MarketMaker::insert(price_type price, size_type size)
{
  id_type id;

  if(!this->_is_running)
    throw invalid_state("market/market-maker is not in a running state");

  id = this->_book->insert_limit_order(BuyNotSell,price,size,this->_callback);

  if(id == 0)
    throw invalid_order("order could not be inserted");
  else
    this->my_orders.insert( orders_value_type(id,
                               order_bndl_type(BuyNotSell,price,size)) );
}


void MarketMaker::_base_callback(callback_msg msg,
                                 id_type id,
                                 price_type price,
                                 size_type size)
{
  order_bndl_type ob;

  switch(msg){
  case callback_msg::fill:
    {
      ob = order_bndl_type(this->my_orders.at(id)); // throw
      this->last_fill_was_buy = std::get<0>(ob);
      this->last_fill_price = std::get<1>(ob);
      this->last_fill_size = std::get<2>(ob);
      this->last_fill_id = id;

      if(size >= this->last_fill_size)
        this->my_orders.erase(id);
    }
    break;
  case callback_msg::cancel:
    this->my_orders.erase(id);
    break;
  case callback_msg::stop:
    this->stop();
    break;
  }
}

market_makers_type MarketMaker::Factory(std::initializer_list<callback_type> il)
{
  market_makers_type mms;
  for( auto& i : il )
    mms.push_back( pMarketMaker(new MarketMaker(i)) );

  return mms;
}

market_makers_type MarketMaker::Factory(unsigned int n)
{
  market_makers_type mms;
  while( n-- )
    mms.push_back( pMarketMaker(new MarketMaker()) );

  return mms;
}

/***/

MarketMaker_Simple1::MarketMaker_Simple1(size_type sz)
  :
    MarketMaker( std::bind(&MarketMaker_Simple1::_callback,this,_1,_2,_3,_4) ),
    _sz(sz)
  {
  }

void MarketMaker_Simple1::start(SimpleOrderbook::LimitInterface *book,
                                price_type implied,
                                price_type incr)
{
  size_type i;
  price_type price;

  my_base_type::start(book,implied,incr);

  for( i = 0, price = implied + 1 ; i < 5; price += incr, ++i )
    this->insert<false>(price,this->_sz);

  for( i = 0, price = implied - 1 ; i < 5; price -= incr, ++i )
    this->insert<true>(price,this->_sz);
}


void MarketMaker_Simple1::_callback(callback_msg msg,
                                    id_type id,
                                    price_type price,
                                    size_type size)
{
  switch(msg){
  case callback_msg::fill:
    { /* THIS inserts 2 orders for every fill(need to regulate)
      if(this->last_fill_was_buy){/*bought*/
        this->insert<true>(price - this->tick, size);
        this->insert<false>(price + (2*this->tick), size);
      }else{ /*sold*/
        this->insert<false>(price + this->tick, size);
        this->insert<true>(price - (2*this->tick), size);
      }
    }
    break;
  case callback_msg::cancel:
    break;
  case callback_msg::stop:
    break;
  }
}

market_makers_type
MarketMaker_Simple1::Factory(std::initializer_list<size_type> il)
{
  market_makers_type mms;
  for( auto& i : il )
    mms.push_back( pMarketMaker(new MarketMaker_Simple1(i)) );

  return mms;
}

market_makers_type MarketMaker_Simple1::Factory(unsigned int n,size_type sz)
{
  market_makers_type mms;
  while( n-- )
    mms.push_back( pMarketMaker(new MarketMaker_Simple1(sz)) );

  return mms;
}

/***/

MarketMaker_Random::MarketMaker_Random(size_type sz_low, size_type sz_high)
  :
    my_base_type( std::bind(&MarketMaker_Random::_callback,this,_1,_2,_3,_4) ),
    _sz_low(sz_low),
    _sz_high(sz_high),
    _rand_engine(this->_gen_seed()),
    _distr(sz_low, sz_high),
    _distr2(1, 5)
  {
  }
MarketMaker_Random::MarketMaker_Random(const MarketMaker_Random& mm)
  :
    my_base_type(mm),
    _sz_low(mm._sz_low),
    _sz_high(mm._sz_high),
    _rand_engine(this->_gen_seed()),
    _distr(mm._sz_low, mm._sz_high),
    _distr2(1, 5)
  {
  }

unsigned long long MarketMaker_Random::_gen_seed()
{
  return (clock_type::now() - MarketMaker_Random::seedtp).count()
         * (unsigned long long)this
         % std::numeric_limits<long>::max();
}

void MarketMaker_Random::start(SimpleOrderbook::LimitInterface *book,
                               price_type implied,
                               price_type incr)
{
  size_type mod, count, i;
  price_type price;

  my_base_type::start(book,implied,incr);

  mod = this->_distr2(this->_rand_engine);
  count = this->_distr2(this->_rand_engine);
  /* insert some random sell-limits */
  for( i = 0, price = implied + 1 ; i < count; price += mod * incr, ++i )
    this->insert<false>(price, this->_distr(this->_rand_engine));
  /* insert some random buy-limits */
  for( i = 0, price = implied - 1 ; i < count; price -= mod * incr, ++i )
    this->insert<true>(price, this->_distr(this->_rand_engine));
}

void MarketMaker_Random::_callback(callback_msg msg,
                                   id_type id,
                                   price_type price,
                                   size_type size)
{
  price_type adj;
  switch(msg){
  case callback_msg::fill:
    {
      adj = this->tick * this->_distr2(this->_rand_engine);
      if(this->last_fill_was_buy) /*bought*/
        this->insert<true>(price - adj, this->_distr(this->_rand_engine));
      else /*sold*/
        this->insert<false>(price + adj, this->_distr(this->_rand_engine));
    }
    break;
  case callback_msg::cancel:
    break;
  case callback_msg::stop:
    break;
  }
}

market_makers_type MarketMaker_Random::Factory(
                     std::initializer_list<std::pair<size_type,size_type>> il)
{
  market_makers_type mms;
  for( auto& i : il )
    mms.push_back( pMarketMaker(new MarketMaker_Random(i.first,i.second)) );

  return mms;
}

market_makers_type MarketMaker_Random::Factory(unsigned int n,
                                               size_type sz_low,
                                               size_type sz_high)
{
  market_makers_type mms;
  while( n-- )
    mms.push_back( pMarketMaker(new MarketMaker_Random(sz_low,sz_high)) );

  return mms;
}
const clock_type::time_point MarketMaker_Random::seedtp = clock_type::now();

};
