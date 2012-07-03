// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file basic/database/schema_generator/Schema.cc
///
/// @brief Construct a database backend independant schema
/// @author Tim Jacobs

//Unit
#include <basic/database/schema_generator/Schema.hh>
#include <basic/database/schema_generator/PrimaryKey.hh>
#include <basic/database/schema_generator/ForeignKey.hh>
#include <basic/database/schema_generator/Column.hh>
#include <basic/database/schema_generator/Index.hh>

#include <basic/database/sql_utils.hh>

// Basic Headers
#include <basic/options/option.hh>
#include <basic/options/keys/inout.OptionKeys.gen.hh>
#include <basic/Tracer.hh>
#include <platform/types.hh>

// Utility Headers
#include <utility/exit.hh>
#include <utility/sql_database/types.hh>

#include <string>
#include <stdio.h>
#include <set>
#include <algorithm>
#include <cctype>

namespace basic{
namespace database{
namespace schema_generator{

using platform::Size;
using std::string;
using std::stringstream;

static basic::Tracer TR("basic.database.schema_generator.Schema");


Schema::Schema(std::string table_name):
table_name_(table_name)
{
	init();
}

Schema::Schema(std::string table_name, PrimaryKey primary_key):
table_name_(table_name),
primary_key_(primary_key)
{
	init();
}

Schema::Schema(
	Schema const & src
) :
	database_mode_(src.database_mode_),
	primary_key_(src.primary_key_),
	columns_(src.columns_),
	foreign_keys_(src.foreign_keys_),
	constraints_(src.constraints_),
	indices_(src.indices_)
{}

void
Schema::init(){
	database_mode_ =
		utility::sql_database::database_mode_from_name(
			basic::options::option[basic::options::OptionKeys::inout::dbms::mode]);

	//Add primary key columns to schema list
	Columns key_columns = primary_key_.columns();
	this->columns_.insert( columns_.end(), key_columns.begin(), key_columns.end() );

	// Table names should all be lower case
	std::transform(
		table_name_.begin(), table_name_.end(), table_name_.begin(),
		(int(*)(int)) std::tolower);

}

void
Schema::add_foreign_key(
	ForeignKey key
){
	this->foreign_keys_.push_back(key);
	//if the foreign key is also a primary key it will have already been added

	Columns key_cols = key.columns();

	for(Size i=1; i <= key_cols.size(); ++i){
		if(!this->columns_.contains(key_cols[i]))
		{
			this->columns_.push_back(key_cols[i]);
		}
	}
}

void
Schema::add_column(
	Column column
){
	//Don't add a column more than once
	if(!this->columns_.contains(column))
	{
		this->columns_.push_back(column);
	}
}

void
Schema::add_constraint(
	ConstraintOP constraint
){
	this->constraints_.push_back(constraint);
}

void
Schema::add_index(
	Index index
) {
	indices_.push_back(index);
}

std::string Schema::print(){
	stringstream schema_string;
	switch(database_mode_){
	case utility::sql_database::DatabaseMode::postgres:
		schema_string << "CREATE TABLE " << table_name_ << "(\n\t";
		break;
	case utility::sql_database::DatabaseMode::mysql:
	case utility::sql_database::DatabaseMode::sqlite3:
		schema_string << "CREATE TABLE IF NOT EXISTS " << table_name_ << "(\n\t";
		break;
	default:
		utility_exit_with_message(
			"Unrecognized database mode: '" + name_from_database_mode(database_mode_) + "'");
	}

	for (Columns::const_iterator it=columns_.begin(); it!=columns_.end(); it++){
		if(it!=columns_.begin()){
			schema_string << ",\n\t";
		}
		schema_string << it->print();
	}

	for(size_t i=1; i<=foreign_keys_.size(); i++){
		schema_string << ",\n\t" << foreign_keys_[i].print();
	}

	Columns const & keys(primary_key_.columns());

	if(keys.size() > 0){
		switch(database_mode_) {
		case utility::sql_database::DatabaseMode::mysql:
		case utility::sql_database::DatabaseMode::postgres:
			schema_string << ",\n\t" << primary_key_.print();
			break;
		case utility::sql_database::DatabaseMode::sqlite3:
			//Prevent adding the primary key twice - this will happen if you have an autoincrementing primary key in sqlite3

			if(!(keys.size()==1 && keys.begin()->auto_increment())){
				schema_string << ",\n\t" << primary_key_.print();
			}
			break;
		default:
			utility_exit_with_message(
				"Unrecognized database mode: '" + name_from_database_mode(database_mode_) + "'");
		}
	}

	for(size_t i=1; i<=constraints_.size(); i++){
		schema_string << ",\n\t" << constraints_[i]->print();
	}

	schema_string << ");\n";

	for(Size i=1; i <= indices_.size(); ++i){
		schema_string << indices_[i].print(table_name_);
		schema_string << "\n";
	}

	return schema_string.str();
}

//Write this schema to the database
void Schema::write(
	utility::sql_database::sessionOP db_session
){
	std::string stmt_string = this->print();

	bool exists=false;

	try{
		//Older versions of postgres do not support "create table if not exists"
		if(database_mode_ == utility::sql_database::DatabaseMode::postgres){
			cppdb::statement exists_stmt;

			if(!(db_session->get_pq_schema() == "")){
				exists_stmt = (*db_session) << "SELECT tablename "
					"FROM pg_catalog.pg_tables "
					"WHERE schemaname = ? AND tablename = ?;";
				exists_stmt.bind(1, db_session->get_pq_schema());
				exists_stmt.bind(2, table_name_);
			} else {
				exists_stmt = (*db_session) << "SELECT tablename "
					"FROM pg_catalog.pg_tables "
					"WHERE tablename = ?;";
				exists_stmt.bind(1, table_name_);
			}

			cppdb::result res = safely_read_from_database(exists_stmt);
			if(res.next()){
				exists=true;
			}
		}
		if(!exists){
			cppdb::statement stmt = (*db_session) << stmt_string;
			safely_write_to_database(stmt);
		}
	} catch (cppdb::cppdb_error e) {
		TR.Error
		<< "ERROR reading schema \n"
		<< stmt_string << std::endl;
		TR.Error << e.what() << std::endl;
		utility_exit();
	}
}

} // schema_generator
} // namespace database
} // namespace utility
