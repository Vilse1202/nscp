#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

#include <net/net.hpp>

namespace nscapi {
	namespace targets {
		struct target_object {

			std::string path;
			std::string alias;
			std::string value;
			std::string parent;
			bool is_template;

			net::url address;
			typedef std::map<std::string,std::string> options_type;
			options_type options;

			std::string to_string() const {
				std::stringstream ss;
				ss << "Target: " << alias;
				ss << ", address: " << get_address();
				ss << ", parent: " << parent;
				ss << ", is_template: " << is_template;
				BOOST_FOREACH(options_type::value_type o, options) {
					ss << ", option[" << o.first << "]: " << o.second;
				}
				return ss.str();
			}
			std::string get_address() const {
				return address.to_string();
			}
			void set_address(std::string value) {
				net::url n = net::parse(value);
				address.apply(n);
			}
			void set_host(std::string value) {
				address.host = value;
			}
			void set_port(int value) {
				address.port = value;
			}
			bool has_option(std::string key) const {
				return options.find(key) != options.end();
			}
			void set_property_int(std::string key, int value) {
				if (key == "port") {
					set_port(value);
				} else 
					options[key] = strEx::s::xtos(value);
			}
			void set_property_bool(std::string key, bool value) {
				options[key] = value?"true":"false";
			}
			void set_property_string(std::string key, std::string value) {
				if (key == "host") {
					set_host(value);
				} else 
					options[key] = value;
			}

			nscapi::protobuf::types::destination_container to_destination_container() const {
				nscapi::protobuf::types::destination_container ret;
				if (!alias.empty())
					ret.id = alias;
				ret.address.apply(address);
				BOOST_FOREACH(const options_type::value_type &kvp, options) {
					ret.data[kvp.first] = kvp.second;
				}
				return ret;
			}

		};
		typedef boost::optional<target_object> optional_target_object;
		typedef std::map<std::string,std::string> targets_type;

		struct target_object_reader {
			typedef target_object object_type;
			static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner);
			static void apply_parent(object_type &object, object_type &parent);
		};

		namespace sh = nscapi::settings_helper;
		struct dummy_custom_reader {
			typedef target_object object_type;
			static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
				settings; 
				proxy; 
				object;
			}
		};

		template<class custom_reader>
		struct split_object_reader {
			typedef target_object object_type;

			static void post_process_object(object_type &object) {
				custom_reader::post_process_target(object);
			}

			static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner) {
				object.address = net::parse(object.value, 0);
				if (object.alias == "default")
					custom_reader::init_default(object);

				nscapi::settings_helper::settings_registry settings(proxy);

				object_type::options_type options;
				settings.path(object.path).add_path()
					(nscapi::settings_helper::string_map_path(&options), 
					"TARGET DEFENITION", "Target definition for: " + object.alias)

					;

				settings.path(object.path).add_key()

					("address", sh::string_fun_key<std::string>(boost::bind(&object_type::set_address, &object, _1)),
					"TARGET ADDRESS", "Target host address")

					("host", sh::string_fun_key<std::string>(boost::bind(&object_type::set_host, &object, _1)),
					"TARGET HOST", "The target server to report results to.", true)

					("port", sh::int_fun_key<int>(boost::bind(&object_type::set_port, &object, _1)),
					"TARGET PORT", "The target server port", true)

					("alias", nscapi::settings_helper::string_key(&object.alias, object.alias),
					"TARGET ALIAS", "The alias for the target", true)

					("parent", nscapi::settings_helper::string_key(&object.parent, "default"),
					"TARGET PARENT", "The parent the target inherits from", true)

					("is template", nscapi::settings_helper::bool_key(&object.is_template, false),
					"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

					;
				custom_reader::add_custom_keys(settings, proxy, object);

				settings.register_all();
				settings.notify();

				BOOST_FOREACH(const object_type::options_type::value_type &kvp, options) {
					if (!object.has_option(kvp.first))
						object.options[kvp.first] = kvp.second;
				}

			}

			static void apply_parent(object_type &object, object_type &parent) {
				object.address.import(parent.address);
				BOOST_FOREACH(object_type::options_type::value_type i, parent.options) {
					if (object.options.find(i.first) == object.options.end())
						object.options[i.first] = i.second;
				}
			}

		};
		template<class custom_reader>
		struct handler : public nscapi::settings_objects::object_handler<target_object, split_object_reader<custom_reader > > {};
		struct helpers {
			static void verify_file(target_object &target, std::string key, std::list<std::string> &errors);
		};

	}
}

