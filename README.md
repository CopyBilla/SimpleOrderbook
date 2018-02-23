## SimpleOrderbook 
- - -

SimpleOrderbook is a C++(11) financial market orderbook and matching engine with a Python extension module.

#### Features 

- market, limit, stop-market, and stop-limit order types
- advanced orders/conditions: ***IN DEVELOPMENT (NOT STABLE)***
    - one-cancels-other (OCO) 
    - one-triggers-other (OTO)
    - fill-or-kill (FOK)
    - bracket
    - trailing stop 
    - bracket /w trailing stop
    - all-or-none (AON) ***(not available yet)***
- advanced condition triggers: ***IN DEVELOPMENT (NOT STABLE)***
    - fill-partial 
    - fill-full 
    - fill-n-percent ***(not available yet)***
- cancel/replace orders by ID
- user-defined callbacks on order execution/cancelation/advanced triggers etc.
- query market state(bid size, volume etc.), dump orders to stdout, view Time & Sales 
- extensible backend resource management(global and type-specific) via factory proxies 
- tick sizing/rounding/math handled implicity by TickPrice\<std::ratio\> objects
- pre-allocation of (some) internals during construction to reduce runtime overhead
- (manually) grow orderbooks as necessary
- access via a CPython extension module

#### Design & Performance

- The 'spine' of the orderbook is a vector which allows:
    - random access and simple pointer/index math internally
    - operations on cached internal pointers/indices of important levels
- The vector is initialized to user-requested size when the book is created but can be grown manually: 
    - all of the price details of stored orders (e.g stop-limit price) are saved as doubles
    - we simply let the STL implementation do its thing, get the new base address, and adjust the internal pointers by the offset
- The vector contains pairs of doubly-linked lists ('chains') which allows:
    - one list for limits and a seperate list for stops
    - the potential to add new chains for advanced orders (e.g AON orders)
    - quick push/pop so order insert/execution is O(C) for basic order types (see below)
    - quick removal from inside the list
- Orders are referenced by ID #s that are generated sequentially and cached - with their respective price level and chain type - in an unordered_map (hash table) allowing for:
    - collission-free O(C) lookup from the cache and O(N) lookup from the chain, so
    - worst case pull/remove (every order is at 1 price level) O(N) compexity viz-a-viz # of active orders
    - best case pull/remove (no more than 1 order at each price level) O(C) complexity viz-a-viz # of active orders
- The effect of advanced orders on all this is currently unknown

##### Basic Order Insert & Execution:

- n orders: 40% limits, 20% markets, 20% stops, 20% stop-limits
- prices are distributed normally around the mid-point w/ a SD 20% of the range
- order sizes are distributed log-normally * 200 using a 0 mean and 1 SD
- buy/sell condition from a simple .50 bernoulli 
- average 10 seperate runs of each
- use a TickRatio of 1/10000

n orders | total time | time per order
---------|------------|----------------
100      | 0.003898   | 0.000039 
1000     | 0.036946   | 0.000037
10000    | 0.300888   | 0.000030 
100000   | 3.173736   | 0.000032 
1000000  | 36.754020  | 0.000037 

##### Basic Order Pull:

- n orders are inserted, ids are stored
    - 50% limits, 25% stops, 25% stop-limits
    - prices are distributed normally around the mid-point w/ a SD 20% of the range
    - order sizes are distributed log-normally * 200 using a 0 mean and 1 SD 
    - buy/sell for limits is dependent on price relative to mid-point (WANT NO TRADES)
    - buy/sell condition for stops from a simple .50 benoulli
- ids are randomly shuffled
- loop through and remove each id
- average 10 seperate runs of each
- use a TickRatio of 1/10000

n orders | total time | time per order 
---------|------------|----------------
100      | 0.001949   | 0.000019
1000     | 0.024924   | 0.000025
10000    | 0.373072   | 0.000037
100000   | 14.051795  | 0.000141
1000000  | 615.977580 | 0.000616

#### Getting Started

- **C++** 

        user@host:/usr/local/SimpleOrderbook$ g++ --std=c++11 -Iinclude -lpthread src/simpleorderbook.cpp src/advanced_order.cpp samples/example_code.cpp -o example_code.out
        user@host:/usr/local/SimpleOrderbook$ ./example_code.out  

- **python**

        user@host:/usr/local/SimpleOrderbook/python$ python setup.py install
        user@host:/usr/local/SimpleOrderbook/python$ python
        >>> import simpleorderbook           

#### Examples
 
        // example_code.cpp

        #include <unordered_map>
        #include "simpleorderbook.hpp"

        std::unordered_map<sob::id_type, sob::id_type> advanced_ids;

        void 
        execution_callback(sob::callback_msg msg, 
                           sob::id_type id1,
                           sob::id_type id2,
                           double price,
                           size_t size);

        void 
        insert_orders(sob::FullInterface *orderbook);

        void 
        insert_advanced_orders(sob::FullInterface *orderbook);

        void
        print_inside_market(sob::QueryInterface *orderbook);

        int
        main(int argc, char* argv[])
        {
            using namespace sob;

            /* 
             * First, we need to create a factory proxy. To support different 
             * orderbook types and different constructor types, in a single factory, 
             * we provide different factory interfaces(proxies).
             *
             * The following will be used for managing orderbooks of (implementation) type:
             *     SimpleOrderbook::SimpleOrderbookImpl< std::ratio<1,4> >
             *
             * - uses the default factory 'create' function via (double,double) constructor
             * - with .25 price intervals       
             * 
             * Proxies (implicitly) restrict default construction and assignment
             */         
            SimpleOrderbook::FactoryProxy<> qt_def_proxy = 
                SimpleOrderbook::BuildFactoryProxy<quarter_tick>();            

           /*
            * This approach(assuming we use the default FactoryProxy) 
            * provides factory interfaces - for each and ANY type of 
            * orderbook - all of the same type; allowing for:
            */           
            std::map<std::string, SimpleOrderbook::FactoryProxy<>> 
            my_factory_proxies = { 
                {"QT", qt_def_proxy},
                {"TT", SimpleOrderbook::BuildFactoryProxy<tenth_tick>()},
                {"HT", SimpleOrderbook::BuildFactoryProxy<std::ratio<1,2>>()}
            };

            /*  
             * Use the factory proxy to create an orderbook that operates 
             * between .25 and 100.00 in .25 increments, returning a pointer
             * to its full interface 
             *
             * NOTE: .create() is built to throw: logic and runtime errors (handle accordingly)
             */ 
            FullInterface *orderbook = my_factory_proxies.at("QT").create(.25, 100.00);                  
            if( !orderbook ){
                // error (this *shouldn't* happen)
                return 1;
            }

            /* 
             * IMPORTANT: internally, each orderbook is managed by a 'resource manager' 
             * behind each proxy AND a global one for ALL proxies/orderbooks. This 
             * allows for the use of that type's proxy member functions OR the global
             * static methods of the SimpleOrderbook container class. (see below)
             */

            /* check if orderbook is being managed by ANY proxy */
            if( !SimpleOrderbook::IsManaged(orderbook) )
            {
                /* get orderbooks being managed by ALL proxies */
                std::vector<FullInterface*> actives = SimpleOrderbook::GetAll();

                std::cerr<< "error: active orderbooks for ALL proxies" << std::endl;
                for( FullInterface *i : actives ){
                    std::cerr<< std::hex << reinterpret_cast<void*>(i) << std::endl;
                }
                return 1;
            }       

            /* check if orderbook is being managed by THIS proxy */
            if( !my_factory_proxies.at("QT").is_managed(orderbook) )
            {
                /* get all orderbooks being managed by THIS proxy */
                std::vector<FullInterface*> actives = my_factory_proxies.at("QT").get_all();

                std::cerr<< "error: active orderbooks for 'QT' proxy" << std::endl;
                for( FullInterface *i : actives ){
                    std::cerr<< std::hex << reinterpret_cast<void*>(i) << std::endl;
                }
                return 1;
            }    

            /* use the full interface (defined below) */
            insert_orders(orderbook);            

            /* use the query interface (defined below) */
            print_inside_market(orderbook);

            /* increase the size of the orderbook */
            dynamic_cast<ManagementInterface*>(orderbook)->grow_book_above(150);

            if( orderbook->min_price() == .25 
                && orderbook->max_price() == 150.00
                && orderbook->tick_size() == .25
                && orderbook->price_to_tick(150.12) == 150.00
                && orderbook->price_to_tick(150.13) == 150.25
                && orderbook->is_valid_price(150.13) == false
                && orderbook->ticks_in_range(.25, 150) == 599 
                && orderbook->tick_memory_required(.25, 150) == orderbook->tick_memory_required() )
            {
                std::cout<< "good orderbook" << std::endl;
            }

            /* use advanced orders (IN DEVELOPEMENT) */
            insert_advanced_orders(orderbook);

            /* 
             * WHEN DONE...
             */
          
            /* use the proxy to destroy the orderbook it created */
            my_factory_proxies.at("QT").destroy(orderbook);

            /* (or) use the global version to destroy ANY orderbook 
               NOTE: orderbook should only be destroyed once (no-op in this case) */
            SimpleOrderbook::Destroy(orderbook);

            /* use the proxy to destroy all the orderbooks it created */
            my_factory_proxies.at("QT").destroy_all();

            /* (or) use the global version to destroy ALL orderbooks */
            SimpleOrderbook::DestroyAll();

            //...
            
            return 0;
        }   

        void 
        execution_callback(sob::callback_msg msg, 
                           sob::id_type id1,
                           sob::id_type id2,
                           double price,
                           size_t size)
        {
            /* if we use OCO order need to be aware of potential ID# change on trigger */
            if( msg == sob::callback_msg::trigger_OCO ){
                std::cout<< "order #" << id1 << " is now #" << id2 << std::endl;
                advanced_ids[id1] = id2;
            }
            std::cout<< msg << " " << id1 << " " << id2 << " " 
                     << price << " " << size << std::endl;
            // define
        }

        void 
        insert_orders(sob::FullInterface *orderbook)
        {
            /* buy 50 @ 49.75 or better */
            sob::id_type id1 = orderbook->insert_limit_order(true, 49.75, 50, execution_callback);
            /* sell 10 @ market */
            sob::id_type id2 = orderbook->insert_market_order(false, 10, execution_callback);

            /* pull orders */
            std::cout<< "pull order #1 (should be true): " << std::boolalpha 
                     << orderbook->pull_order(id1) << std::endl;
            std::cout<< "pull order #2 (should be false): " 
                     << orderbook->pull_order(id2) << std::endl;
        }

        void 
        insert_advanced_orders(sob::FullInterface *orderbook)
        {
            /* create a OCO (one-cancels-other) buy-limit/sell-limit order */

            /* first create an AdvancedOrderTicket */
            auto aot = sob::AdvancedOrderTicketOCO::build_limit(false, 50.00, 100, 
                            sob::condition_trigger::fill_partial);

            /* then insert it to the standard interface 
               NOTE: it will call back when the condition is triggered,
                     the new order id will be in field 'id2' */
            sob::id_type id = orderbook->insert_limit_order(true, 49.50, 100, execution_callback, aot);
            advanced_ids[id] = id;
            std::cout<< "ORDER #" << id << ": " << orderbook->get_order_info(id) << std::endl;

            /* if either order fills the other is canceled (and ID# may have changed)*/
            orderbook->insert_market_order(true, 50);
            sob::order_info oi = orderbook->get_order_info(advanced_ids[id]);
            std::cout<< "ORDER #" << advanced_ids[id] << ": " << oi << std::endl;

            orderbook->dump_buy_limits();
            orderbook->dump_sell_limits();
        }

        void
        print_inside_market(sob::QueryInterface *orderbook)
        {
            std::cout<< "BID: " << orderbook->bid_size() 
                     << " @ " << orderbook->bid_price() << std::endl;
            std::cout<< "ASK: " << orderbook->ask_size() 
                     << " @ " << orderbook->ask_price() << std::endl;
            std::cout<< "LAST: " << orderbook->last_size() 
                     << " @ " << orderbook->last_price() << std::endl;
        }

<br>
    
        // python

        >>> import simpleorderbook as sob
        >>> ob = sob.SimpleOrderbook(sob.SOB_QUARTER_TICK, .25, 100) 
        >>> print("%f to %f by %f" % (ob.min_price(), ob.max_price(), ob.tick_size()))
        0.250000 to 100.000000 by 0.250000
        >>> cb = lambda a,b,c,d,e: print("+ msg:%i id_old:%i id_new:%i price:%f size:%i" % (a,b,c,d,e))
        >>> ob.buy_limit(limit=20.0, size=100, callback=cb) 
        1
        >>> ob.buy_limit(20.25, 100, cb)
        2
        >>> ob.buy_limit(21.0, 100, cb)
        3
        >>> ob.bid_depth(10)
        {20.0: 100, 20.25: 100, 21.0: 100}
        >>> ob.bid_size()
        100
        >>> ob.total_bid_size()
        300
        >>> ob.sell_stop(stop=21.0, size=50) 
        4
        >>> ob.sell_stop_limit(stop=21.0, limit=20.75, size=100, callback=cb) 
        5
        >>> ob.dump_sell_stops()
        *** (sell) stops ***
        21 <S 50 @ MKT #4>  <S 100 @ 20.750000 #5> 
        >>> ob.dump_buy_limits()
        *** (buy) limits ***
        21 <100 #3> 
        20.25 <100 #2> 
        20 <100 #1> 
        >>> ob.sell_market(50) 
        + msg:1 id_old:3 id_new:3 price:21.0 size:50 # callback from order #3 (1 == FILL) 
        + msg:2 id_old:5 id_new:8 price:20.75 size:100 # callback from order #5 (2 == STOP-TO-LIMIT)
        + msg:1 id_old:3 id_new:3 price:21.0 size:50 # callback from order #3 (1 == FILL)
        6
        >>> for ts in ob.time_and_sales():
        ...  ts
        ... 
        ('Sat Jan 13 18:51:31 2018', 21.0, 50)
        ('Sat Jan 13 18:51:31 2018', 21.0, 50)
        >>> ob.market_depth(10)
        {20.0: (100, 1), 20.25: (100, 1), 20.75: (100, -1)}  ## 1 == SIDE_BID, -1 == SIDE_ASK
        >>> ob.dump_buy_limits()
        *** (buy) limits ***
        20.25 <100 #2> 
        20 <100 #1> 
        >>> ob.pull_order(id=2)
        + msg:0 id_old:2 id_new:2 price:0.0 size:0 # callback from order #2 (0 == CANCEL)
        True
        >>> ob.replace_with_buy_limit(id=1, limit=20.50, size=500) # callback=None
        + msg:0 id_old:1 id_new:1 price:0.0 size:0 # callback from order #1 (0 == CANCEL)
        9
        >>> ob.dump_buy_limits()
        *** (buy) limits ***
        20.5 <500 #9> 

#### Licensing & Warranty
*SimpleOrderbook is released under the GNU General Public License(GPL); a copy (LICENSE.txt) should be included. If not, see http://www.gnu.org/licenses. The author reserves the right to issue current and/or future versions of SimpleOrderbook under other licensing agreements. Any party that wishes to use SimpleOrderbook, in whole or in part, in any way not explicitly stipulated by the GPL, is thereby required to obtain a separate license from the author. The author reserves all other rights.*

*This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.*
