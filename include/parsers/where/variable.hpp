#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {		
	
		struct variable {
			variable(std::string name) : name(name), int_ptr_id(0), string_ptr_id(0) {}

			bool bind(value_type type, filter_handler handler);
			long long get_int(filter_handler handler) const;
			std::string get_string(filter_handler handler) const;
			std::string get_name() const { return name; }
			bool is_bound_int() const { return int_ptr_id != 0; }
			bool is_bound_string() const { return string_ptr_id != 0; }
			bool is_bound() const { return string_ptr_id != 0||int_ptr_id != 0; }

			variable( const variable& other ) : name(other.name), int_ptr_id(other.int_ptr_id), string_ptr_id(other.string_ptr_id) {}
			const variable& operator=( const variable& other ) {
				name = other.name;
				int_ptr_id = other.int_ptr_id;
				string_ptr_id = other.string_ptr_id;
				return *this;
			}
			
		private:
			std::string name;
			unsigned int int_ptr_id;
			unsigned int string_ptr_id;
			variable() : int_ptr_id(0), string_ptr_id(0) {}
		};
	}
}