#pragma once
#include <eosio/chain/code_object.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/whitelisted_intrinsics.hpp>
#include <eosio/chain/exceptions.hpp>
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"

namespace eosio { namespace chain {

   class apply_context;
   class wasm_runtime_interface;
   class controller;

   struct wasm_exit {
      int32_t code = 0;
   };

   namespace webassembly { namespace common {
      class intrinsics_accessor;

      class root_resolver : public Runtime::Resolver {
      public:
         // The non-default constructor puts root_resolver in a mode where it does validation, i.e. only allows "env" imports.
         // This mode is used by the generic validating code that runs during setcode, where we only want "env" to pass.
         // The default constructor is used when no validation is required such as when the wavm runtime needs to
         // allow linkage to the intrinsics and the injected functions.

         root_resolver() {}

         root_resolver( const whitelisted_intrinsics_type& whitelisted_intrinsics )
         :whitelisted_intrinsics(&whitelisted_intrinsics)
         {}

         bool resolve(const string& mod_name,
                      const string& export_name,
                      IR::ObjectType type,
                      Runtime::ObjectInstance*& out) override
         { try {
            bool fail = false;

            if( whitelisted_intrinsics != nullptr ) {
               // Protect access to "private" injected functions; so for now just simply allow "env" since injected
               // functions are in a different module.
               EOS_ASSERT( mod_name == "env", wasm_exception,
                           "importing from module that is not 'env': ${module}.${export}",
                           ("module",mod_name)("export",export_name) );

               // Only consider imports that are in the whitelisted set of intrinsics
               fail = !is_intrinsic_whitelisted( *whitelisted_intrinsics, export_name );
            }

            // Try to resolve an intrinsic first.
            if( !fail && Runtime::IntrinsicResolver::singleton.resolve( mod_name, export_name, type, out ) ) {
               return true;
            }

            EOS_THROW( wasm_exception, "${module}.${export} unresolveable",
                      ("module",mod_name)("export",export_name) );
            return false;
         } FC_CAPTURE_AND_RETHROW( (mod_name)(export_name) ) }

      protected:
         const whitelisted_intrinsics_type* whitelisted_intrinsics = nullptr;
      };
   } }

   /**
    * @class wasm_interface
    *
    */
   class wasm_interface {
      public:
         enum class vm_type {
            wavm,
            wabt
         };

         wasm_interface(vm_type vm);
         ~wasm_interface();

         //validates code -- does a WASM validation pass and checks the wasm against EOSIO specific constraints
         static void validate(const controller& control, const bytes& code);

         //Calls apply or error on a given code
         void apply(const code_object& code, apply_context& context);

         //Immediately exits currently running wasm. UB is called when no wasm running
         void exit();

      private:
         unique_ptr<struct wasm_interface_impl> my;
         friend class eosio::chain::webassembly::common::intrinsics_accessor;
   };

} } // eosio::chain

namespace eosio{ namespace chain {
   std::istream& operator>>(std::istream& in, wasm_interface::vm_type& runtime);
}}

FC_REFLECT_ENUM( eosio::chain::wasm_interface::vm_type, (wavm)(wabt) )
