#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/resource_limits.hpp>

#include "../contracts.hpp"
#include "../eosio.system_tester.hpp"

using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

// #ifndef TESTER
// #ifdef NON_VALIDATING_TEST
// #define TESTER tester
// #else
// #define TESTER validating_tester
// #endif
// #endif

#define CONTRACT_ACCOUNT name("exchange")

namespace eosio {
   namespace chain {
      // Add this in symbol.hpp (line 172)
      inline bool operator== (const extended_symbol& lhs, const extended_symbol& rhs)
      {
         return ( lhs.sym.value() | (uint128_t(lhs.contract.value) << 64) ) == ( rhs.sym.value() | (uint128_t(rhs.contract.value) << 64) );
      }
      inline bool operator!= (const extended_symbol& lhs, const extended_symbol& rhs)
      {
         return ( lhs.sym.value() | (uint128_t(lhs.contract.value) << 64) ) != ( rhs.sym.value() | (uint128_t(rhs.contract.value) << 64) );
      }
      inline bool operator< (const extended_symbol& lhs, const extended_symbol& rhs)
      {
         return ( lhs.sym.value() | (uint128_t(lhs.contract.value) << 64) ) < ( rhs.sym.value() | (uint128_t(rhs.contract.value) << 64) );
      }
      inline bool operator> (const extended_symbol& lhs, const extended_symbol& rhs)
      {
         return ( lhs.sym.value() | (uint128_t(lhs.contract.value) << 64) ) > ( rhs.sym.value() | (uint128_t(rhs.contract.value) << 64) );
      }
   }
}

namespace eosio_system {

   class exchange_tester : public eosio_system_tester {
   public:
      friend inline bool operator< (const extended_symbol& lhs, const extended_symbol& rhs);

      static name exchange_account;
      exchange_tester() {
         deploy_contract();
      }

      abi_serializer deploy_code(name account, const std::vector<uint8_t>& wasm, const std::vector<char>& abiname) {
         set_code(account, wasm);
         set_abi(account, abiname.data());

         produce_blocks();
         const auto& accnt = control->db().get<account_object, by_name>(account);

         abi_serializer abi_ser;
         abi_def        abi;
         REQUIRE(abi_serializer::to_abi(accnt.abi, abi));
         abi_ser.set_abi(abi, abi_serializer_max_time);
         return abi_ser;
      }

      void deploy_contract() {
         // create_account(exchange_account);
         create_account_with_resources(exchange_account, config::system_account_name, 1000000);
         produce_blocks(2);
         deploy_code(exchange_account, contracts::exchange_wasm(), contracts::exchange_abi());

         eos_token = token(this, name("eosio.token"), asset(10000000000000, symbol(4,"EOS")));
         eos_token.issue(name("eosio.token"), asset(10000000000000, symbol(4,"EOS")));

         add_code_permission(exchange_account);
      }

      action_result push_action_ex(account_name actor, const name& code, const action_name& acttype, const variant_object& data) {
         return base_tester::push_action(get_action(code, acttype, {permission_level{actor, config::active_name}}, data), uint64_t(actor));
      }

      action_result push_action_ex(const std::vector<permission_level>& perms,
                                   const name&                          code,
                                   const action_name&                   acttype,
                                   const variant_object&                data,
                                   const std::vector<permission_level>& signers) {
         signed_transaction trx;
         trx.actions.emplace_back(get_action(code, acttype, perms, data));
         set_transaction_headers(trx);
         for (const auto& auth : signers) {
            trx.sign(get_private_key(auth.actor, auth.permission.to_string()), control->get_chain_id());
         }
         try {
            // print_action_console(push_transaction(trx));
            push_transaction(trx);
         } catch (const fc::exception& ex) {
            edump((ex.to_detail_string()));
            return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
            // return error(ex.to_detail_string());
         }
         produce_block();
         BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
         return success();
      }

      /*
      *  Helper Functions
      */

      void add_code_permission(name account) {
         const auto priv_key = this->get_private_key(account.to_string(), "active");
         const auto pub_key  = priv_key.get_public_key();

         this->set_authority(account, name("active"), authority(1, {key_weight{pub_key, 1}}, {{permission_level{account, name("eosio.code")}, 1}}),
                             "owner");
      }

      transaction make_transaction(const fc::variants& actions) {
         variant pretty_trx = fc::mutable_variant_object()("expiration", "2020-01-01T00:30")("ref_block_num", 2)("ref_block_prefix", 3)(
             "max_net_usage_words", 0)("max_cpu_usage_ms", 0)("delay_sec", 0)("actions", actions);
         transaction trx;
         abi_serializer::from_variant(pretty_trx, trx, get_resolver(), abi_serializer_max_time);
         return trx;
      }

      transaction make_transaction(vector<action>&& actions) {
         transaction trx;
         trx.actions = std::move(actions);
         set_transaction_headers(trx);
         return trx;
      }

      transaction
      make_transaction_with_data(account_name code, name action, const vector<permission_level>& perms, const fc::variant& data) {
         return make_transaction({fc::mutable_variant_object()("account", code)("name", action)("authorization", perms)("data", data)});
      }

      /*
      *  Contract Actions
      */

      action_result transfer(name from, name to, const asset& amount, std::string memo) {
         return push_action_ex(from, name("eosio.token"), name("transfer"),
                               mutable_variant_object()("from", from)("to", to)("quantity", amount)("memo", memo));
      }

      /*
      *  TABLES
      */

      abi_serializer get_serializer() {
         const auto& acnt = control->get_account(CONTRACT_ACCOUNT);
         auto        abi  = acnt.get_abi();
         return abi_serializer(abi, abi_serializer_max_time);
      }

      std::map<extended_symbol, int64_t> get_exchange_account_balance(account_name acc) {
         std::map<extended_symbol, int64_t> balances;
         
         vector<char> data        = get_row_by_account(CONTRACT_ACCOUNT, acc, name("exaccounts"), acc);

         if ( data.empty() ) {
            CHECK( 1 == 1);
            return balances;
         }
         else {
            auto var = get_serializer().binary_to_variant("exaccount", data, abi_serializer_max_time);
            auto owner = var["owner"].as<name>();
            CHECK( owner == name("alice"));

            // auto balance2 = var["balances"];
            // CHECK( balance2 == variant() );
            // CHECK( balance2 == mvo()("key", "eosio.token")("value", "2000") );

            auto balance = var["balances"].as<vector<fc::variant>>();
            CHECK( balance.size() == 1);
            

            struct tmp_struct{
               extended_symbol key;
               int64 value;
            };
            std::vector<variant> p2;
            auto x = balance[0].as<tmp_struct>();


            // std::vector<pair<extended_symbol, int64_t>> my_vector{balance.size()};
            // std::transform(balance.begin(), balance.end(), my_vector.begin(), [](auto &v) { return v.template as<pair<extended_symbol, int64_t>>();});

            // CHECK( my_vector.begin()->second == 20000);
            // CHECK(balance.begin()->second == 20000);

            // balances.insert(balance_vector.begin(), balance_vector.end());
            return balances;
         }
      }

      /*
      *  eosio.token Contract Interface
      */

      struct token {
         exchange_tester* tester_;
         name           issuer_;
         symbol         sym_;

         token() {}

         token(exchange_tester* tester, name issuer, asset max_supply)
            : tester_(tester)
            , issuer_(issuer)
            , sym_(max_supply.get_symbol()) {
            create_currency(name("eosio.token"), issuer_, max_supply);
         }
         
         void create_currency( name contract, name issuer, asset maxsupply ) {
            tester_->push_action_ex(issuer, name("eosio.token"), name("create"),
                                    mutable_variant_object()("issuer", issuer)("maximum_supply", maxsupply));
         }
         void issue(name to, asset quantity, std::string memo = "") {
            tester_->push_action_ex(issuer_, name("eosio.token"), name("issue"),
                                    mutable_variant_object()("to", to)("quantity", quantity)("memo", memo));
         }
         void open(name owner, name ram_payer) {
            REQUIRE(tester_->push_action_ex(ram_payer, name("eosio.token"), name("open"),
                                            mutable_variant_object()("owner", owner)("symbol", sym_)("ram_payer", ram_payer)) ==
                    tester_->success());
         }
         abi_serializer get_serializer() {
            const auto& acnt = tester_->control->get_account(name("eosio.token"));
            auto        abi  = acnt.get_abi();
            return abi_serializer(abi, abi_serializer_max_time);
         }

         asset get_account_balance(account_name acc) {
            auto         symbol_code = sym_.to_symbol_code().value;
            vector<char> data        = tester_->get_row_by_account(name("eosio.token"), acc, name("accounts"), symbol_code);
            return data.empty() ? asset(0, sym_)
                                : get_serializer().binary_to_variant("account", data, abi_serializer_max_time)["balance"].as<asset>();
         }

         asset get_supply() {
            auto         symbol_code = sym_.to_symbol_code().value;
            vector<char> data        = tester_->get_row_by_account(name("eosio.token"), symbol_code, name("stat"), symbol_code);
            return data.empty() ? asset(0, sym_)
                                : get_serializer().binary_to_variant("currency_stats", data, abi_serializer_max_time)["supply"].as<asset>();
         }
      };

      token eos_token;
   };

   name exchange_tester::exchange_account = CONTRACT_ACCOUNT;

} // namespace eosio_system


TEST_CASE_FIXTURE(eosio_system::exchange_tester, "deposit") try {

   GIVEN("alice has 5 EOS") {

      create_account_with_resources(name("alice"), config::system_account_name, 1000000);
      transfer(name("eosio.token"), name("alice"), asset(50000, symbol(4,"EOS")), "memo");

      REQUIRE(eos_token.get_account_balance(name("alice")) == asset(50000, symbol(4,"EOS")));

      WHEN("alice deposits 2 EOS into the exchange") {

         CHECK(success() == transfer(name("alice"), exchange_account, asset(20000, symbol(4,"EOS")), "eosio.token"));

         THEN("alice would have 2 EOS in her exchange account") {
            CHECK(eos_token.get_account_balance(name("alice")) == asset(30000, symbol(4,"EOS")));
            CHECK(eos_token.get_account_balance(exchange_account) == asset(20000, symbol(4,"EOS")));

            extended_symbol return_sym;
            return_sym.sym = symbol(4, "EOS");
            return_sym.contract = name("eosio.token");

            std::map<extended_symbol, int64_t> return_balance;
            return_balance.emplace(return_sym, 20000);

            std::map<extended_symbol, int64_t> alice_ex_balance = get_exchange_account_balance(name("alice"));

         }

      }

   }

} FC_LOG_AND_RETHROW()


TEST_CASE_FIXTURE(eosio_system::exchange_tester, "withdraw") try {


} FC_LOG_AND_RETHROW()