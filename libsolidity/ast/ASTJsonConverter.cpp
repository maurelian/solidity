/*
    This file is part of solidity.

    solidity is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    solidity is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Lefteris <lefteris@ethdev.com>
 * @date 2015
 * Converts the AST into json format
 */

#include <libsolidity/ast/ASTJsonConverter.h>
#include <boost/algorithm/string/join.hpp>
#include <libdevcore/UTF8.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/interface/Exceptions.h>

using namespace std;

namespace dev
{
namespace solidity
{

ASTJsonConverter::ASTJsonConverter(bool _legacy, map<string, unsigned> _sourceIndices):
	m_legacy(_legacy),
	m_sourceIndices(_sourceIndices)
{
}


void ASTJsonConverter::setJsonNode(
	ASTNode const& _node,
	string const& _nodeName,
	initializer_list<pair<string const, Json::Value>>&& _attributes
)
{
	ASTJsonConverter::setJsonNode(
		_node,
		_nodeName,
		std::vector<pair<string const, Json::Value>>(std::move(_attributes))
	);
}
  
void ASTJsonConverter::setJsonNode(
	ASTNode const& _node,
	string const& _nodeType,
	std::vector<pair<string const, Json::Value>>&& _attributes
)
{
	m_currentValue = Json::objectValue;
	m_currentValue["id"] = Json::UInt64(_node.id());
	m_currentValue["src"] = sourceLocationToString(_node.location());
	m_currentValue["nodeType"] = _nodeType;
	for (auto& e: _attributes)
		m_currentValue[e.first] = std::move(e.second);
}

string ASTJsonConverter::sourceLocationToString(SourceLocation const& _location) const
{
	int sourceIndex{-1};
	if (_location.sourceName && m_sourceIndices.count(*_location.sourceName))
		sourceIndex = m_sourceIndices.at(*_location.sourceName);
	int length = -1;
	if (_location.start >= 0 && _location.end >= 0)
		length = _location.end - _location.start;
	return std::to_string(_location.start) + ":" + std::to_string(length) + ":" + std::to_string(sourceIndex);
}

string ASTJsonConverter::namePathToString(std::vector<ASTString> const& _namePath) const //do we need this? eigentlich ist der boostbefehl doch ganz schoen
{
	return boost::algorithm::join(_namePath, ".");
}

void ASTJsonConverter::print(ostream& _stream, ASTNode const& _node)
{
	_stream << toJson(_node);
}

Json::Value ASTJsonConverter::toJson(ASTNode const& _node)
{
	_node.accept(*this);
	return std::move(m_currentValue);
}

bool ASTJsonConverter::visit(SourceUnit const& _node)
{
	Json::Value exportedSymbols = Json::objectValue;
	for (auto const& sym: _node.annotation().exportedSymbols)
	{
		exportedSymbols[sym.first] = Json::arrayValue;
		for (Declaration const* overload: sym.second)
			exportedSymbols[sym.first].append(overload->id());
	}
	setJsonNode(
		_node,
		"SourceUnit",
		{
			make_pair("absolutePath", _node.annotation().path),
			make_pair("exportedSymbols", move(exportedSymbols)),
			make_pair("nodes", toJson(_node.nodes()))
		}
	);
	return false;
}

bool ASTJsonConverter::visit(PragmaDirective const& _node)
{
	Json::Value literals(Json::arrayValue);
	for (auto const& literal: _node.literals())
		literals.append(literal);
	setJsonNode(
		_node,
		"PragmaDirective",
		{{"literals", literals}}
	);
	return false;
}

bool ASTJsonConverter::visit(ImportDirective const& _node)
{
	std::vector<pair<string const, Json::Value>> attributes = {
		make_pair("file", _node.path()),
		make_pair("absolutePath", _node.annotation().absolutePath),
		make_pair("SourceUnit", _node.annotation().sourceUnit->id()),
		make_pair("scope", idOrNull(_node.scope()))
	};
	attributes.push_back(make_pair("unitAlias", _node.name()));
	Json::Value symbolAliases(Json::arrayValue);
	for (auto const& symbolAlias: _node.symbolAliases())
	{
		Json::Value tuple(Json::objectValue);
		solAssert(symbolAlias.first, "");
		tuple["foreign"] = symbolAlias.first->id();
		tuple["local"] =  symbolAlias.second ? Json::Value(*symbolAlias.second) : Json::nullValue;
		symbolAliases.append(tuple);
	}
	attributes.push_back( make_pair("symbolAliases", symbolAliases));
	setJsonNode(_node, "ImportDirective", std::move(attributes));
	return false;
}

bool ASTJsonConverter::visit(ContractDefinition const& _node) //todo add inheritancespecifier
{
	Json::Value linearizedBaseContracts(Json::arrayValue);
	for (auto const& baseContract: _node.annotation().linearizedBaseContracts)
		linearizedBaseContracts.append(Json::UInt64(baseContract->id()));
	Json::Value contractDependencies(Json::arrayValue);
	for (auto const& dependentContract: _node.annotation().contractDependencies)
		contractDependencies.append(Json::UInt64(dependentContract->id()));
	setJsonNode(_node, "ContractDefinition", {
		make_pair("name", _node.name()),
		make_pair("isLibrary", _node.isLibrary()),
		make_pair("fullyImplemented", _node.annotation().isFullyImplemented),
		make_pair("linearizedBaseContracts", linearizedBaseContracts),
		make_pair("contractDependencies", contractDependencies),
		make_pair("nodes", toJson(_node.subNodes())),
		make_pair("scope", idOrNull(_node.scope()))
	});
	return false;
}

bool ASTJsonConverter::visit(InheritanceSpecifier const& _node)
{

	//??this node never shows up!
	cout << "imin" << endl; //apparently this function is not even entered....
	// assumed usage:
	// import contract.sol <- with bar-contract
	// contract foo is bar {...
	setJsonNode(_node, "InheritanceSpecifier", {
		make_pair("baseName", toJson(_node.name())),
		make_pair("arguments", toJson(_node.arguments()))
	});
	return false;
}

bool ASTJsonConverter::visit(UsingForDirective const& _node)
{
	setJsonNode(_node, "UsingForDirective", {
		make_pair("libraryNames", toJson(_node.libraryName())),
		make_pair("typeName", _node.typeName() ? toJson(*_node.typeName()) : Json::Value("*"))
	});
	return false;
}

bool ASTJsonConverter::visit(StructDefinition const& _node)
{
	setJsonNode(_node, "StructDefinition", {
		make_pair("name", _node.name()),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("canonicalName", _node.annotation().canonicalName),
		make_pair("members", toJson(_node.members())),
		make_pair("scope",idOrNull(_node.scope()))
	});
	return false;
}

bool ASTJsonConverter::visit(EnumDefinition const& _node)
{
	setJsonNode(_node, "EnumDefinition", {
		make_pair("name", _node.name()),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("canonicalName", _node.annotation().canonicalName),
		make_pair("members", toJson(_node.members())),
		make_pair("scope", idOrNull(_node.scope()))
	});
	return false;
}

bool ASTJsonConverter::visit(EnumValue const& _node)
{
	setJsonNode(_node, "EnumValue", { make_pair("name", _node.name()) });
	return false;
}

bool ASTJsonConverter::visit(ParameterList const& _node)
{
	setJsonNode(_node, "ParameterList", {
		make_pair("parameters", toJson(_node.parameters()))
	});
	return false;
}

bool ASTJsonConverter::visit(FunctionDefinition const& _node)
{
	std::vector<pair<string const, Json::Value>> attributes = {
		make_pair("name", _node.name()),
		make_pair("constant", _node.isDeclaredConst()),
		make_pair("payable", _node.isPayable()),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("parameters",	toJson(_node.parameterList())),
		make_pair("isConstructor", _node.isConstructor()),
		make_pair("returnParameters", toJson((*_node.returnParameterList()))),
		make_pair("modifiers", toJson(_node.modifiers())),
		make_pair("body", _node.isImplemented() ? toJson(_node.body()) : Json::nullValue),
		make_pair("isImplemented", _node.isImplemented()),
		make_pair("scope", idOrNull(_node.scope()))
	};
	setJsonNode(_node, "FunctionDefinition", std::move(attributes));
	return false;
}

bool ASTJsonConverter::visit(VariableDeclaration const& _node)
{
	std::vector<pair<string const, Json::Value>> attributes = {
		make_pair("name", _node.name()),
		make_pair("type", type(_node)),
		make_pair("constant", _node.isConstant()),
		make_pair("storageLocation", location(_node.referenceLocation())),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("value", _node.value() ? toJson(*_node.value()) : Json::nullValue),
		make_pair("scope", idOrNull(_node.scope())),
		make_pair("typeName", toJsonOrNull(_node.typeName())),
	};
	if (m_inEvent)
		attributes.push_back(make_pair("indexed", _node.isIndexed()));
	setJsonNode(_node, "VariableDeclaration", std::move(attributes));
	return false;
}

bool ASTJsonConverter::visit(ModifierDefinition const& _node)
{
	setJsonNode(_node, "ModifierDefinition", {
		make_pair("name", _node.name()),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("parameters", toJson(_node.parameterList())),
		make_pair("body", toJson(_node.body().statements()))
	});
	return false;
}

bool ASTJsonConverter::visit(ModifierInvocation const& _node)
{
	setJsonNode(_node, "ModifierInvocation", {
		make_pair("name", _node.name()->name()),
		make_pair("arguments", toJson(_node.arguments()))
	});
	return false;
}

bool ASTJsonConverter::visit(TypeName const&)
//Review: TypeName is abstract, so this is never called?
{
	return false;
}

bool ASTJsonConverter::visit(EventDefinition const& _node)
{
	m_inEvent = true;
	setJsonNode(_node, "EventDefinition", {
		make_pair("name", _node.name()),
		make_pair("parameters", toJson(_node.parameterList())),
		make_pair("isAnonymous", _node.isAnonymous()),
		make_pair("scope", idOrNull(_node.scope()))
	});
	return false;
}

bool ASTJsonConverter::visit(ElementaryTypeName const& _node)
//Review: I added nothing, but dont understand what the elementaryTypeNameTokenClass holds
//(especcialy first and seconNumber....
{
	setJsonNode(_node, "ElementaryTypeName", { make_pair("name", _node.typeName().toString()) });
	return false;
}

bool ASTJsonConverter::visit(UserDefinedTypeName const& _node) //review?
{
	setJsonNode(_node, "UserDefinedTypeName", {
		make_pair("name", namePathToString(_node.namePath())),
		make_pair( "referencedDeclaration", idOrNull(_node.annotation().referencedDeclaration)),
		make_pair("contractScope", idOrNull(_node.annotation().contractScope))
	});
	return false;
}

bool ASTJsonConverter::visit(FunctionTypeName const& _node)
{
	setJsonNode(_node, "FunctionTypeName", {
		make_pair("payable", _node.isPayable()),
		make_pair("visibility", visibility(_node.visibility())),
		make_pair("constant", _node.isDeclaredConst()),
		make_pair("parameterTypes", toJson(_node.parameterTypes())),
		make_pair("returnParameterTypes", toJson(_node.returnParameterTypes()))
	});
	return false;
}

bool ASTJsonConverter::visit(Mapping const& _node)
{
	setJsonNode(_node, "Mapping", {
		make_pair("keyType", toJson(_node.keyType())),
		make_pair("valueType", toJson(_node.valueType()))
	});
	return false;
}

bool ASTJsonConverter::visit(ArrayTypeName const& _node)
{
	setJsonNode(_node, "ArrayTypeName", {
		make_pair("baseType", toJson(_node.baseType())),
		make_pair("length", toJsonOrNull(_node.length()))//,
		//make_pair("type", _node.annotation().type->id())  //->Review: is this necessary? (comp. struct.sol for example)
	});
	return false;
}

bool ASTJsonConverter::visit(InlineAssembly const& _node) //TODO > print
{
	setJsonNode(_node, "InlineAssembly", {
//		make_pair("operations", toJson(_node.operations().toString()))
		//TODO add annotation
	});
	return false;
}

bool ASTJsonConverter::visit(Block const& _node)
{
	setJsonNode(_node, "Block", {
		make_pair("statements", toJson(_node.statements()))
	});
	return false;
}

bool ASTJsonConverter::visit(PlaceholderStatement const& _node)
{
	setJsonNode(_node, "PlaceholderStatement", {});
	return false;
}

bool ASTJsonConverter::visit(IfStatement const& _node)
{
	setJsonNode(_node, "IfStatement", {
		make_pair("condition", toJson(_node.condition())),
		make_pair("trueBody", toJson(_node.trueStatement())),
		make_pair("falseBody", toJsonOrNull(_node.falseStatement()))
	});
	return false;
}

bool ASTJsonConverter::visit(WhileStatement const& _node)
{
	setJsonNode(
		_node,
		_node.isDoWhile() ? "DoWhileStatement" : "WhileStatement",
		{
			make_pair("condition", toJson(_node.condition())),
			make_pair("body", toJson(_node.body()))
		}
	);
	return false;
}

bool ASTJsonConverter::visit(ForStatement const& _node)
{
	setJsonNode(_node, "ForStatement", {
		make_pair("initExpression", toJsonOrNull(_node.initializationExpression())),
		make_pair("condition", toJsonOrNull(_node.condition())),
		make_pair("loopExpression", toJsonOrNull(_node.loopExpression())),
		make_pair("body", toJson(_node.body()))
	});
	return false;
}

bool ASTJsonConverter::visit(Continue const& _node)
{
	setJsonNode(_node, "Continue", {});
	return false;
}

bool ASTJsonConverter::visit(Break const& _node)
{
	setJsonNode(_node, "Break", {});
	return false;
}

bool ASTJsonConverter::visit(Return const& _node)
{
	setJsonNode(_node, "Return", {
		make_pair("expression", toJsonOrNull(_node.expression())),
		make_pair("functionReturnParameters", idOrNull(_node.annotation().functionReturnParameters))
	});
	return false;
}

bool ASTJsonConverter::visit(Throw const& _node)
{
	setJsonNode(_node, "Throw", {});;
	return false;
}

bool ASTJsonConverter::visit(VariableDeclarationStatement const& _node)
{
	Json::Value varDecs(Json::arrayValue);
	for (auto const& v: _node.annotation().assignments)
	{
		varDecs.append(idOrNull(v));
	}
	setJsonNode(_node, "VariableDeclarationStatement", {
		make_pair("declarationIDs", varDecs),
		make_pair("declarations", toJson(_node.declarations())),
		make_pair("initialValue", toJsonOrNull(_node.initialValue()))
	});
	return false;
}

bool ASTJsonConverter::visit(ExpressionStatement const& _node) //a!
{
	setJsonNode(_node, "ExpressionStatement", {
		make_pair("expression", toJson(_node.expression()))//,
//		make_pair("argumentTypes")// TODO
	});
	return false;
}

bool ASTJsonConverter::visit(Conditional const& _node) //expression
{
	setJsonNode(_node, "Conditional", {
		make_pair("condition", toJson(_node.condition())),
		make_pair("trueExpression", toJson(_node.trueExpression())),
		make_pair("falseExpression", toJson(_node.falseExpression())),
		    });
	return false;
}

bool ASTJsonConverter::visit(Assignment const& _node) //expression
{
	setJsonNode(_node, "Assignment", {
		make_pair("operator", Token::toString(_node.assignmentOperator())),
		make_pair("type", type(_node)),
		make_pair("leftHandSide", toJson(_node.leftHandSide())),
		make_pair("rightHandSide", toJson(_node.rightHandSide())),
	});
	return false;
}

bool ASTJsonConverter::visit(TupleExpression const& _node) //expressions
{
	setJsonNode(_node, "TupleExpression",{
		make_pair("isInlineArray", Json::Value(_node.isInlineArray())),
		make_pair("components", toJson(_node.components()))
	});
	return false;
}

bool ASTJsonConverter::visit(UnaryOperation const& _node) //expression
{
	setJsonNode(_node, "UnaryOperation", {
		make_pair("prefix", _node.isPrefixOperation()),
		make_pair("operator", Token::toString(_node.getOperator())),
		make_pair("type", type(_node)),
		make_pair("subExpression", toJson(_node.subExpression()))
	});
	return false;
}

bool ASTJsonConverter::visit(BinaryOperation const& _node)
{
	setJsonNode(_node, "BinaryOperation", {
		make_pair("operator", Token::toString(_node.getOperator())),
		make_pair("type", type(_node)),
		make_pair("commonType", _node.annotation().commonType->toString())
	});
	return false;
}

bool ASTJsonConverter::visit(FunctionCall const& _node)
{
	Json::Value names(Json::arrayValue);
	for (auto const& name: _node.names())
		names.append(Json::Value(*name));
	setJsonNode(_node, "FunctionCall", {
		make_pair("type_conversion", _node.annotation().isTypeConversion),
		make_pair("isStructContstructorCall", _node.annotation().isStructConstructorCall),
		make_pair("type", type(_node)),
		make_pair("arguments", toJson(_node.arguments())),
		make_pair("expression", toJson(_node.expression())),
		make_pair("names", names)
	});
	return false;
}

bool ASTJsonConverter::visit(NewExpression const& _node) //expressionstuff
{
	setJsonNode(_node, "NewExpression", {
		make_pair("type", type(_node)),
		make_pair("typeName", toJson(_node.typeName()))
	});
	return false;
}

bool ASTJsonConverter::visit(MemberAccess const& _node)
{
	setJsonNode(_node, "MemberAccess", {
		make_pair("memberName", _node.memberName()),
		make_pair("type", type(_node)),
		make_pair("expression", toJson(_node.expression())),
		make_pair("referencedDeclaration", idOrNull(_node.annotation().referencedDeclaration))
	});
	return false;
}

bool ASTJsonConverter::visit(IndexAccess const& _node)   //expressionstuff?
{
	setJsonNode(_node, "IndexAccess", {
		make_pair("type", type(_node)),
		make_pair("baseExpression", toJson(_node.baseExpression())),
		make_pair("indexExpression", toJsonOrNull(_node.indexExpression()))
	});

	return false;
}

bool ASTJsonConverter::visit(Identifier const& _node)
{
	Json::Value overloads(Json::arrayValue);
	for (auto const& dec: _node.annotation().overloadedDeclarations)
		overloads.append(Json::Value(dec->id()));
	setJsonNode(_node, "Identifier", {
		make_pair("value", _node.name()),
		make_pair("type", type(_node)),
		make_pair("referencedDeclaration", idOrNull(_node.annotation().referencedDeclaration)),
		make_pair("overloadedDeclarations", overloads)
	});
	return false;
}

bool ASTJsonConverter::visit(ElementaryTypeNameExpression const& _node) //what 2 incl. form expression annotation
{
	setJsonNode(_node, "ElementaryTypeNameExpression", {
		make_pair("value", _node.typeName().toString()),
		make_pair("type", type(_node)),
		make_pair("isConstant", _node.annotation().isConstant),//entire expression part from here down
		make_pair("isPure", _node.annotation().isPure),
		make_pair("isLValue", _node.annotation().isLValue),
		make_pair("lValueRequested", _node.annotation().lValueRequested)
		//TODO how to include the arguments which are a list of typepointers?
	});
	return false;
}

bool ASTJsonConverter::visit(Literal const& _node) //would need express annos
{
	char const* tokenString = Token::toString(_node.token());
	Json::Value value{_node.value()};
	if (!dev::validateUTF8(_node.value()))
		value = Json::nullValue;
	Token::Value subdenomination = Token::Value(_node.subDenomination());
	setJsonNode(_node, "Literal", {
		make_pair("token", tokenString ? tokenString : Json::Value()),
		make_pair("value", value),
		make_pair("hexvalue", toHex(_node.value())),
		make_pair(
			"subdenomination",
			subdenomination == Token::Illegal ?
			Json::nullValue :
			Json::Value{Token::toString(subdenomination)}
		),
		make_pair("type", type(_node))
	});
	return false;
}


void ASTJsonConverter::endVisit(EventDefinition const&)
{
	m_inEvent = false;
}

string ASTJsonConverter::visibility(Declaration::Visibility const& _visibility)
{
	switch (_visibility)
	{
	case Declaration::Visibility::Private:
		return "private";
	case Declaration::Visibility::Internal:
		return "internal";
	case Declaration::Visibility::Public:
		return "public";
	case Declaration::Visibility::External:
		return "external";
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown declaration visibility."));
	}
}

string ASTJsonConverter::location(VariableDeclaration::Location _location)
{
	switch (_location)
	{
	case VariableDeclaration::Location::Default:
		return "default";
	case VariableDeclaration::Location::Storage:
		return "storage";
	case VariableDeclaration::Location::Memory:
		return "memory";
	default:
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unknown declaration location."));
	}
}

string ASTJsonConverter::type(Expression const& _expression)
{
	return _expression.annotation().type ? _expression.annotation().type->toString() : "Unknown";
}

string ASTJsonConverter::type(VariableDeclaration const& _varDecl)
{
	return _varDecl.annotation().type ? _varDecl.annotation().type->toString() : "Unknown";
}

}
}
