// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "database.hpp"

#include <iostream>
#include <sstream>

#include "malloc.hpp"
#include "showmsg.hpp"
#include "utilities.hpp"

using namespace rathena;

bool YamlDatabase::nodeExists( const ryml::ConstNodeRef& node, const std::string& name ){
	return (node.num_children() > 0 && node.has_child(c4::to_csubstr(name)));
}

bool YamlDatabase::nodesExist( const ryml::ConstNodeRef& node, std::initializer_list<const std::string> names ){
	bool missing = false;

	for( const std::string& name : names ){
		if( !this->nodeExists( node, name ) ){
			ShowError( "Missing mandatory node \"%s\".\n", name.c_str() );
			missing = true;
		}
	}

	if( missing ){
		this->invalidWarning( node, "At least one mandatory node did not exist.\n" );
		return false;
	}

	return true;
}

bool YamlDatabase::verifyCompatibility( const ryml::Tree& tree ){
	if( !this->nodeExists( tree.rootref(), "Header" ) ){
		ShowError( "No database \"Header\" was found.\n" );
		return false;
	}

	const ryml::ConstNodeRef& headerNode = tree["Header"];

	std::string tmpType;

	if( !this->asString( headerNode, "Type", tmpType ) ){
		return false;
	}

	if( this->type != tmpType ){
		ShowError( "Database type mismatch: %s != %s.\n", this->type.c_str(), tmpType.c_str() );
		return false;
	}

	uint16 tmpVersion;

	if( !this->asUInt16( headerNode, "Version", tmpVersion ) ){
		return false;
	}

	if( tmpVersion != this->version ){
		if( tmpVersion > this->version ){
			ShowError( "Database version %hu is not supported. Maximum version is: %hu\n", tmpVersion, this->version );
			return false;
		}else if( tmpVersion >= this->minimumVersion ){
			ShowWarning( "Database version %hu is outdated and should be updated. Current version is: %hu\n", tmpVersion, this->version );
			ShowWarning( "Reduced compatibility with %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), this->currentFile.c_str() );
		}else{
			ShowError( "Database version %hu is not supported anymore. Minimum version is: %hu\n", tmpVersion, this->minimumVersion );
			return false;
		}
	}

	return true;
}

bool YamlDatabase::load(){
	bool ret = this->load( this->getDefaultLocation() );

	this->loadingFinished();

	return ret;
}

bool YamlDatabase::reload(){
	this->clear();

	return this->load();
}

bool YamlDatabase::load(const std::string& path) {
	ShowStatus("Loading '" CL_WHITE "%s" CL_RESET "'..." CL_CLL "\r", path.c_str());
	FILE* f = fopen(path.c_str(), "r");
	if (f == nullptr) {
		ShowError("Failed to open %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), path.c_str());
		return false;
	}
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	char* buf = (char *)aMalloc(size+1);
	rewind(f);
	size_t real_size = fread(buf, sizeof(char), size, f);
	// Zero terminate
	buf[real_size] = '\0';
	fclose(f);

	ryml::Tree tree;

	try{
		event_handler = ryml::EventHandlerTree{};
		parser = ryml::Parser{&event_handler};
		tree = ryml::parse_in_arena(&parser, c4::to_csubstr(path), c4::to_csubstr(buf));
	}catch( const std::runtime_error& e ){
		ShowError( "Failed to load %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), path.c_str() );
		ShowError( "There is likely a syntax error in the file.\n" );
		ShowError( "Error message: %s\n", e.what() );
		aFree(buf);
		return false;
	}

	// Required here already for header error reporting
	this->currentFile = path;

	if (!this->verifyCompatibility(tree)){
		ShowError("Failed to verify compatibility with %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), this->currentFile.c_str());
		aFree(buf);
		return false;
	}

	const ryml::ConstNodeRef& header = tree["Header"];

	if( this->nodeExists( header, "Clear" ) ){
		bool clear;

		if( this->asBool( header, "Clear", clear ) && clear ){
			this->clear();
		}
	}

	this->parse( tree );

	this->parseImports( tree );

	aFree(buf);
	return true;
}

void YamlDatabase::loadingFinished(){
	// Does nothing by default, just for hooking
}

void YamlDatabase::parse( const ryml::Tree& tree ){
	uint64 count = 0;

	if( this->nodeExists( tree.rootref(), "Body" ) ){
		const ryml::ConstNodeRef& bodyNode = tree["Body"];
		size_t childNodesCount = bodyNode.num_children();
		const char* fileName = this->currentFile.c_str();
#ifdef DEBUG
		size_t childNodesProgressed = 0;
#endif

		ShowStatus("Loading '" CL_WHITE "%" PRIdPTR CL_RESET "' entries in '" CL_WHITE "%s" CL_RESET "'\n", childNodesCount, fileName);

		for( const ryml::ConstNodeRef &node : bodyNode ){
			count += this->parseBodyNode( node );
#ifdef DETAILED_LOADING_OUTPUT
			ShowStatus( "Loading [%" PRIdPTR "/%" PRIdPTR "] entries from '" CL_WHITE "%s" CL_RESET "'" CL_CLL "\r", ++childNodesProgressed, childNodesCount, fileName );
#endif
		}

		ShowStatus( "Done reading '" CL_WHITE "%" PRIu64 CL_RESET "' entries in '" CL_WHITE "%s" CL_RESET "'" CL_CLL "\n", count, fileName );
	}
}

void YamlDatabase::parseImports( const ryml::Tree& rootNode ){
	if( this->nodeExists( rootNode.rootref(), "Footer" ) ){
		const ryml::ConstNodeRef& footerNode = rootNode["Footer"];

		if( this->nodeExists( footerNode, "Imports") ){
			const ryml::ConstNodeRef& importsNode = footerNode["Imports"];

			for( const ryml::ConstNodeRef &node : importsNode ){
				std::string importFile;

				if( !this->asString( node, "Path", importFile ) ){
					continue;
				}

				if( this->nodeExists( node, "Mode" ) ){
					std::string mode;

					if( !this->asString( node, "Mode", mode ) ){
						continue;
					}

#ifdef RENEWAL
					std::string compiledMode = "Renewal";

					// RENEWAL mode with RENEWAL_ASPD off, load pre-re ASPD
#ifndef RENEWAL_ASPD
					if (importFile.find("job_aspd.yml") != std::string::npos)
						compiledMode = "Prerenewal";
#endif
#else
					std::string compiledMode = "Prerenewal";
#endif

					if( compiledMode != mode ){
						// Skip this import
						continue;
					}
				}

				if (this->nodeExists(node, "Generator")) {
					bool isGenerator;
					if (!this->asBool(node, "Generator", isGenerator)) {
						continue;
					}
					if (!(shouldLoadGenerator && isGenerator))
						continue; // skip import
				}

				this->load( importFile );
			}
		}
	}
}

template <typename R> bool YamlDatabase::asType( const ryml::ConstNodeRef& node, const std::string& name, R& out ){
	if( this->nodeExists( node, name ) ){
		const ryml::ConstNodeRef& dataNode = node[c4::to_csubstr(name)];

		if (dataNode.val_is_null()) {
			this->invalidWarning(node, "Node \"%s\" is missing a value.\n", name.c_str());
			return false;
		}

		try{
			dataNode >> out;
		}catch( std::runtime_error const& ){
			this->invalidWarning( node, "Node \"%s\" cannot be parsed as %s.\n", name.c_str(), typeid( R ).name() );
			return false;
		}

		return true;
	}else{
		this->invalidWarning( node, "Missing node \"%s\".\n", name.c_str() );
		return false;
	}
}

bool YamlDatabase::asBool(const ryml::ConstNodeRef& node, const std::string &name, bool &out) {
	const ryml::ConstNodeRef& targetNode = node[c4::to_csubstr(name)];

	if (targetNode.val_is_null()) {
		this->invalidWarning(node, "Node \"%s\" is missing a value.\n", name.c_str());
		return false;
	}

	std::string str;

	targetNode >> str;

	util::tolower( str );

	if( str == "true" ){
		out = true;
		return true;
	}else if( str == "false" ){
		out = false;
		return true;
	}else{
		this->invalidWarning( targetNode, "Unknown boolean value: \"%s\".\n", str.c_str() );
		return false;
	}
}

bool YamlDatabase::asInt16( const ryml::ConstNodeRef& node, const std::string& name, int16& out ){
	return asType<int16>( node, name, out);
}

bool YamlDatabase::asUInt16(const ryml::ConstNodeRef& node, const std::string& name, uint16& out) {
	return asType<uint16>(node, name, out);
}

bool YamlDatabase::asInt32(const ryml::ConstNodeRef& node, const std::string &name, int32 &out) {
	return asType<int32>(node, name, out);
}

bool YamlDatabase::asUInt32(const ryml::ConstNodeRef& node, const std::string &name, uint32 &out) {
	return asType<uint32>(node, name, out);
}

bool YamlDatabase::asInt64(const ryml::ConstNodeRef& node, const std::string &name, int64 &out) {
	return asType<int64>(node, name, out);
}

bool YamlDatabase::asUInt64(const ryml::ConstNodeRef& node, const std::string &name, uint64 &out) {
	return asType<uint64>(node, name, out);
}

bool YamlDatabase::asFloat(const ryml::ConstNodeRef& node, const std::string &name, float &out) {
	return asType<float>(node, name, out);
}

bool YamlDatabase::asDouble(const ryml::ConstNodeRef& node, const std::string &name, double &out) {
	return asType<double>(node, name, out);
}

bool YamlDatabase::asString(const ryml::ConstNodeRef& node, const std::string &name, std::string &out) {
	return asType<std::string>(node, name, out);
}

bool YamlDatabase::asUInt16Rate( const ryml::ConstNodeRef& node, const std::string& name, uint16& out, uint16 maximum ){
	if( this->asUInt16( node, name, out ) ){
		if( out > maximum ){
			this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" with value %" PRIu16 " exceeds maximum of %" PRIu16 ".\n", name.c_str(), out, maximum );

			return false;
		}else if( out == 0 ){
			this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" needs to be at least 1.\n", name.c_str() );

			return false;
		}else{
			return true;
		}
	}else{
		return false;
	}
}

bool YamlDatabase::asUInt32Rate( const ryml::ConstNodeRef& node, const std::string& name, uint32& out, uint32 maximum ){
	if( this->asUInt32( node, name, out ) ){
		if( out > maximum ){
			this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" with value %" PRIu32 " exceeds maximum of %" PRIu32 ".\n", name.c_str(), out, maximum );

			return false;
		}else if( out == 0 ){
			this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" needs to be at least 1.\n", name.c_str() );

			return false;
		}else{
			return true;
		}
	}else{
		return false;
	}
}

int32 YamlDatabase::getLineNumber(const ryml::ConstNodeRef& node) {
	return parser.source().has_str() ? (int32)node.location(parser).line : 0;
}

int32 YamlDatabase::getColumnNumber(const ryml::ConstNodeRef& node) {
	return parser.source().has_str() ? (int32)node.location(parser).col : 0;
}

void YamlDatabase::invalidWarning( const ryml::ConstNodeRef& node, const char* fmt, ... ){
	va_list ap;

	va_start(ap, fmt);

	// Remove any remaining garbage of a previous loading line
	ShowMessage( CL_CLL );
	// Print the actual error
	_vShowMessage( MSG_ERROR, fmt, ap );

	va_end(ap);

	ShowError( "Occurred in file '" CL_WHITE "%s" CL_RESET "' on line %d and column %d.\n", this->currentFile.c_str(), this->getLineNumber(node), this->getColumnNumber(node));

#ifdef DEBUG
	std::cout << node;
#endif
}

std::string YamlDatabase::getCurrentFile(){
	return this->currentFile;
}

void YamlDatabase::setGenerator(bool shouldLoad) {
	shouldLoadGenerator = shouldLoad;
}

// 0.16 split errors into basic/parse/visit callbacks whose exception types
// no longer derive from std::runtime_error. Register the same throwing
// behavior on all three so database loading keeps failing with
// std::runtime_error exactly as before (callers catch it).
void on_yaml_error( c4::csubstr msg, const ryml::ErrorDataBasic&, void* ){
	throw std::runtime_error( std::string( msg.str, msg.len ) );
}

void on_yaml_error( c4::csubstr msg, const ryml::ErrorDataParse&, void* ){
	throw std::runtime_error( std::string( msg.str, msg.len ) );
}

void on_yaml_error( c4::csubstr msg, const ryml::ErrorDataVisit&, void* ){
	throw std::runtime_error( std::string( msg.str, msg.len ) );
}

void do_init_database(){
	ryml::Callbacks callbacks;
	callbacks.set_error_basic( on_yaml_error )
		.set_error_parse( on_yaml_error )
		.set_error_visit( on_yaml_error );
	ryml::set_callbacks( callbacks );
}
