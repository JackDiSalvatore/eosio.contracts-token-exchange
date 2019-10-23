#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/resource_limits.hpp>

#include "../contracts.hpp"

using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

#ifndef TESTER
#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif
#endif

#define CONTRACT_ACCOUNT N(exampletoken)

namespace eosio_system {

   class ex_token_tester : public TESTER {
   public:
      static name token_account;
      ex_token_tester() {
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
         create_account(token_account);
         produce_blocks(2);
         deploy_code(token_account, contracts::token_wasm(), contracts::token_abi());
         add_code_permission(token_account);
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
         REQUIRE(true == chain_has_transaction(trx.id()));
         return success();
      }

      void add_code_permission(name account) {
         const auto priv_key = this->get_private_key(account.to_string(), "active");
         const auto pub_key  = priv_key.get_public_key();

         this->set_authority(account, N(active), authority(1, {key_weight{pub_key, 1}}, {{permission_level{account, N(eosio.code)}, 1}}),
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
      *  ACTIONS
      */

      action_result create(name   issuer,
                           asset  maximum_supply) {
         return push_action_ex(issuer, CONTRACT_ACCOUNT, N(create),
                               mutable_variant_object()("issuer", issuer)("maximum_supply", maximum_supply));
      }

      action_result issue(name to, const asset& quantity, std::string memo) {
         return push_action_ex(CONTRACT_ACCOUNT, CONTRACT_ACCOUNT, N(issue),
                               mutable_variant_object()("to", to)("quantity", quantity)("memo", memo));
      }

      action_result transfer(name from, name to, const asset& quantity, std::string memo) {
         return push_action_ex(from, CONTRACT_ACCOUNT, N(transfer),
                               mutable_variant_object()("from", from)("to", to)("quantity", quantity)("memo", memo));
      }

      /*
      *  TABLES
      */

      abi_serializer get_serializer() {
         const auto& acnt = control->get_account(CONTRACT_ACCOUNT);
         auto        abi  = acnt.get_abi();
         return abi_serializer(abi, abi_serializer_max_time);
      }

      asset get_account_balance(account_name acc, symbol sym) {
         auto         symbol_code = sym.to_symbol_code().value;
         vector<char> data        = get_row_by_account(CONTRACT_ACCOUNT, acc, N(accounts), symbol_code);
         return data.empty() ? asset(0, sym)
                              : get_serializer().binary_to_variant("account", data, abi_serializer_max_time)["balance"].as<asset>();
      }

      asset get_supply(symbol sym) {
         auto         symbol_code = sym.to_symbol_code().value;
         vector<char> data        = get_row_by_account(CONTRACT_ACCOUNT, symbol_code, N(stat), symbol_code);
         return data.empty() ? asset(0, sym)
                              : get_serializer().binary_to_variant("currency_stats", data, abi_serializer_max_time)["supply"].as<asset>();
      }

   };

   name ex_token_tester::token_account  = CONTRACT_ACCOUNT;

} // namespace eosio_system

TEST_CASE_FIXTURE(eosio_system::ex_token_tester, "make some cash") try {

   create_account(N(alice));
   create_account(N(bob));

   create(token_account, asset(1000000000, symbol(4,"TEST")));
   issue(token_account, asset(100000, symbol(4,"TEST")), "issuance");
   CHECK(get_supply(symbol(4,"TEST")) == asset(100000, symbol(4,"TEST")));

   transfer(token_account, N(alice), asset(50000, symbol(4,"TEST")), "memo");
   CHECK(get_account_balance(N(alice),symbol(4,"TEST")) == asset(50000, symbol(4,"TEST")));

} FC_LOG_AND_RETHROW()
