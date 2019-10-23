#include <eosio.token/eosio.token.hpp>
#include <cmath>

#define CONTRACT_ACCOUNT "exchange"_n

namespace eosio {

   struct token_transfer {
      name from;
      name to;
      extended_asset quantity;
      string memo;
   };

   /**
    *  This contract enables users to create an exchange between any pair of
    *  standard currency types. A new exchange is created by funding it with
    *  an equal value of both sides of the order book and giving the issuer
    *  the initial shares in that orderbook.
    *
    *  To prevent exessive rounding errors, the initial deposit should include
    *  a sizeable quantity of both the base and quote currencies and the exchange
    *  shares should have a quantity 100x the quantity of the largest initial
    *  deposit.
    *
    *  Users must deposit funds into the exchange before they can trade on the
    *  exchange.
    *
    *  Each time an exchange is created a new currency for that exchanges market
    *  maker is also created. This currencies supply and symbol must be unique and
    *  it uses the token contract's tables to manage it.
    */
   class [[eosio::contract("token.exchange")]] exchange : public eosio::contract {
   private:

   /**
    *  Each user has their own account with the exchange contract that keeps track
    *  of how much a user has on deposit for each extended asset type. The assumption
    *  is that storing a single flat map of all balances for a particular user will
    *  be more practical than breaking this down into a multi-index table sorted by
    *  the extended_symbol.  
    */
   struct [[eosio::table]] exaccount {
      name                                 owner;
      std::map<extended_symbol, int64_t>   balances;

      uint64_t primary_key() const { return owner.value; }
   };

   typedef eosio::multi_index<"exaccounts"_n, exaccount> exaccounts;


   /**
    *  Provides an abstracted interface around storing balances for users. This class
    *  caches tables to make multiple accesses effecient.
    */
   // struct exchange_accounts {
   //    exchange_accounts( name code ) : _self( code ){}

   //    void adjust_balance( name owner, extended_asset delta ) {

   //       auto table = exaccounts_cache.find( owner );
   //       if( table == exaccounts_cache.end() ) {
   //          table = exaccounts_cache.emplace( _self, exaccounts( _self, owner.value )  ).first;
   //       }

   //       auto useraccounts = table->second.find( owner.value );
   //       if( useraccounts == table->second.end() ) {
   //          table->second.emplace( _self, [&]( auto& exa ){
   //            exa.owner = owner;
   //            exa.balances[delta.get_extended_symbol()] = delta.quantity.amount;
   //            check( delta.quantity.amount >= 0, "overdrawn balance 1" );
   //          });
   //       } else {
   //          table->second.modify( useraccounts, same_payer, [&]( auto& exa ) {
   //            const auto& b = exa.balances[delta.get_extended_symbol()] += delta.quantity.amount;
   //            check( b >= 0, "overdrawn balance 2" );
   //          });
   //       }

   //    }

   //    private: 
   //       name _self;
   //       /**
   //        *  Keep a cache of all accounts tables we access
   //        */
   //       std::map<name, exaccounts> exaccounts_cache;
   // };


   void _adjust_balance( name owner, extended_asset delta ) {
      exaccounts _exaccounts_table(get_self(), owner.value);

      auto useraccounts = _exaccounts_table.find( owner.value );
      if( useraccounts == _exaccounts_table.end() ) {
         _exaccounts_table.emplace( get_self(), [&]( auto& exa ){
            exa.owner = owner;
            exa.balances[delta.get_extended_symbol()] = delta.quantity.amount;
            check( delta.quantity.amount >= 0, "overdrawn balance 1" );
         });
      } else {
         _exaccounts_table.modify( useraccounts, same_payer, [&]( auto& exa ) {
            const auto& b = exa.balances[delta.get_extended_symbol()] += delta.quantity.amount;
            check( b >= 0, "overdrawn balance 2" );
         });
      }

   }

   name              _self;
   // token             _excurrencies;
   // exchange_accounts _accounts;


   public:
      exchange( name receiver, name code, datastream<const char*> ds )
      :contract( receiver, code, ds )//,
      // _excurrencies(receiver),
      // _accounts( get_self() )
      {}

      [[eosio::action]]
      void deposit( name from, extended_asset quantity );

      [[eosio::action]]
      void withdraw( name  from, extended_asset quantity );

      [[eosio::on_notify("eosio.token::transfer")]]
      void transfer(name from, name to, asset quantity, string memo);

   };
} // namespace eosio
