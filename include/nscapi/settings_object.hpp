#pragma once

#include <map>
#include <list>
#include <string>

#include <boost/any.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <settings/client/settings_client_interface.hpp>

namespace nscapi {

	namespace settings_objects {
		class default_object_type {
			std::string path;
			std::string alias;
			std::string value;
			std::string parent;
		};

		template<class object_type>
		class default_object_reader {
			static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner) {}
			static void apply_parent(object_type &object, object_type &parent) {}
			static void post_process_object(object_type &object) {}
			static void init_default(object_type &object) {}
		};


		template<class t_object_type, class object_reader>
		struct object_handler : boost::noncopyable {
			typedef boost::optional<t_object_type> optional_object;
			typedef std::map<std::string, t_object_type> object_list_type;
			typedef object_handler<t_object_type, object_reader> my_type;

			object_list_type object_list;
			object_list_type template_list;

			void add_missing(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string alias, std::string value, bool is_template = false) {
				if (has_object(alias))
					return;
				add(proxy, path, alias, value, is_template);
			}

			std::list<std::string> get_object_key_list() const {
				std::list<std::string> ret;
				BOOST_FOREACH(const typename object_list_type::value_type &t, object_list) {
					ret.push_back(t.first);
				}
				return ret;
			}
			std::list<t_object_type> get_object_list() const {
				std::list<t_object_type> ret;
				BOOST_FOREACH(const typename object_list_type::value_type &t, object_list) {
					ret.push_back(t.second);
				}
				return ret;
			}
			bool has_objects() const {
				return !object_list.empty();
			}

			t_object_type add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string alias, std::string value, bool is_template = false) {
				optional_object previous = find_object(alias);
				if (previous) {
					t_object_type p = *previous;
					return p;
				}
				t_object_type object;
				object.alias = alias;
				object.value = value;
				object.path = path + "/" + alias;
				object.parent = "default";
				object.is_template = false;

				std::list<std::string> keys = proxy->get_keys(object.path);
				object_reader::read_object(proxy, object, keys.empty());


				if (!object.parent.empty() && object.parent != alias && object.parent != object.alias) {
					t_object_type parent;
					optional_object tmp = find_object(object.parent);
					if (!tmp) {
						parent = add(proxy, path, object.parent, "", true);
					} else {
						parent = *tmp;
					}
					object_reader::apply_parent(object, parent);
				}
				if (is_template || object.is_template)
					add_template(object);
				else
					add_object(object);
				return object;
			}

			void rebuild(boost::shared_ptr<nscapi::settings_proxy> proxy) {
				std::list<t_object_type> tmp;
				BOOST_FOREACH(const typename object_list_type::value_type &t, object_list) {
					tmp.push_back(t.second);
				}
				object_list.clear();
				BOOST_FOREACH(const t_object_type &o, tmp) {
					std::string::size_type pos = o.path.find_last_of("/");
					if (pos == std::string::npos)
						continue;
					add(proxy, o.path.substr(0, pos-1), o.path.substr(pos), o.host);
				}
			}

			optional_object find_object(const std::string alias) const {
				typename object_list_type::const_iterator cit = object_list.find(alias);
				if (cit != object_list.end())
					return optional_object(cit->second);
				cit = template_list.find(alias);
				if (cit != template_list.end())
					return optional_object(cit->second);
				return optional_object();
			}

			bool has_object(std::string alias) const {
				typename object_list_type::const_iterator cit = object_list.find(alias);
				if (cit != object_list.end())
					return true;
				cit = template_list.find(alias);
				if (cit != template_list.end())
					return true;
				return false;
			}

			bool empty() const {
				return object_list.empty();
			}


			std::string to_string() {
				std::stringstream ss;
				ss << "Objects: ";
				BOOST_FOREACH(const typename object_list_type::value_type &t, object_list) {
					ss << ", " << t.first << " = {" << t.second.to_string() + "} ";
				}
				ss << "Templates: ";
				BOOST_FOREACH(const typename object_list_type::value_type &t, template_list) {
					ss << ", " << t.first << " = {" << t.second.to_string() + "} ";
				}
				return ss.str();
			}

			void add_object(t_object_type object) {
				object_reader::post_process_object(object);
				typename object_list_type::iterator cit = template_list.find(object.alias);
				if (cit != template_list.end())
					template_list.erase(cit);
				object_list[object.alias] = object;
			}
			void add_template(t_object_type object) {
				typename object_list_type::const_iterator cit = object_list.find(object.alias);
				if (cit != object_list.end())
					return;
				object_reader::post_process_object(object);
				template_list[object.alias] = object;
			}
		};
	}
}

