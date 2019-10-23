#include <math.h>
#include <token.exchange/token.exchange.hpp>

namespace eosio {


   void exchange::deposit( name from, extended_asset quantity ) {

      check( quantity.quantity.is_valid(), "invalid quantity" );

      action(
         permission_level{ from, "exchange"_n },
         "eosio.token"_n,
         "transfer"_n,
         std::make_tuple( from, get_self(), quantity, std::string("deposit") )
      ).send();

      // _accounts.adjust_balance( from, quantity );
   }


   void exchange::withdraw( name from, extended_asset quantity ) {
      require_auth( from );

      check( quantity.quantity.is_valid(), "invalid quantity" );
      check( quantity.quantity.amount >= 0, "cannot withdraw negative balance" ); // Redundant? inline_transfer will fail if quantity is not positive.
      // _accounts.adjust_balance( from, -quantity );

      action(
         permission_level{ get_self(), "active"_n },
         "eosio.token"_n,
         "transfer"_n,
         std::make_tuple( get_self(), from, quantity, std::string("withdraw") )
      ).send();
   }


   void exchange::transfer( name from, name to, asset quantity, string memo ) {
      // if( code == _self )
      //    _excurrencies.on( t );

      if ( memo.empty() || !memo.compare("deposit") || !memo.compare("withdraw") )
         return;

      if( to == get_self() ) {
         auto a = extended_asset(quantity, name(memo));
         check( a.quantity.is_valid(), "invalid quantity in transfer" );
         check( a.quantity.amount != 0, "zero quantity is disallowed in transfer");
         // _accounts.adjust_balance( from, a );
         _adjust_balance( from, a );
      }
   }


} /// namespace eosio
// EOSIO_DISPATCH( eosio::exchange, (deposit)(withdraw)(transfer) )
