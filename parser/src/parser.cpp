#include <parser.hpp>

namespace simpleparser {

    struct OperatorEntry {
        string mName;
        size_t mPrecedence;
    };

    static map<string, OperatorEntry> sOperators {
        // Precedence 0 is reserved for "no operator".
        {"+", OperatorEntry{"+", 1}},
        {"-", OperatorEntry{"-", 1}},
        {"*", OperatorEntry{"*", 10}},
        {"/", OperatorEntry{"/", 10}},
    };

    Parser::Parser() {
        mTypes["void"] = Type("void", VOID);
        mTypes["int"] = Type("int", INT32);
        mTypes["unsigned"] = Type("unsigned int", UINT32);
        mTypes["char"] = Type("signed char", INT8);
        mTypes["uint8_t"] = Type("uint8_t", UINT8);
        mTypes["double"] = Type("double", DOUBLE); 
    }
    
    void Parser::parse(vector<Token> &tokens) {
        mEndToken = tokens.end();
        mCurrentToken = tokens.begin();

        while (mCurrentToken != tokens.end()) {
            if (expectFunctionDefintion()) {

            } else {
                cerr << "Unknown identifier: " << mCurrentToken->mText << endl;
                ++mCurrentToken;
            }
        }
    }

    bool Parser::expectFunctionDefintion() {
        vector<Token>::iterator parseStart = mCurrentToken;

        optional<Type> possibleType = expectType();

        if (possibleType.has_value()) { 
            optional<Token> possibleName = expectIdentifier();

            if (possibleName.has_value()) {
                optional<Token> possibleOperator = expectOperator("(");

                if (possibleOperator.has_value()) {
                   
                    FunctionDefintion func;
                    func.mName = possibleName->mText;

                    while(!expectOperator(")").has_value()) {
                        optional<Type> possibleParamType = expectType();
                        if (!possibleParamType.has_value()) {
                            throw runtime_error("Expected a type at start of argument list.");
                        }
                        optional<Token> possibleVariableName = expectIdentifier();

                        ParameterDefinition param;
                        param.mType = possibleParamType->mName;
                        if (possibleVariableName.has_value()) {
                            param.mName = possibleVariableName->mText ;
                        }

                        func.mParameters.push_back(param);

                        if (expectOperator(")").has_value()) {
                            break;
                        }

                        if (!expectOperator(",").has_value()) {
                            throw runtime_error("Expected ',' to separate parameters or ')' to indicate end of argument list.");
                            break;
                        }
                    }

                    optional<vector<Statement>> statements = parseFunctionBody();
                    if (!statements.has_value()) {
                        mCurrentToken = parseStart;
                        return false;
                    }
                    func.mStatements.insert(func.mStatements.begin(), statements->begin(), statements->end());

                    mFunctions[func.mName] = func;

                    return true; 

                } else {
                    mCurrentToken = parseStart;
                }
            } else {
                mCurrentToken = parseStart;
            }
        }
        
        return false;
    }

    optional<Token> Parser::expectIdentifier(const string &name) {
        if (mCurrentToken == mEndToken) { return nullopt; }
        if (mCurrentToken->mType != IDENTIFIER) { return nullopt; }
        if (!name.empty() && mCurrentToken->mText != name) { return nullopt; }

        Token returnToken = *mCurrentToken;
        ++mCurrentToken;
        return returnToken;
    }

    optional<Token> Parser::expectOperator(const string &name) {
        if (mCurrentToken == mEndToken) { return nullopt; }
        if (mCurrentToken->mType != OPERATOR) { return nullopt; }
        if (!name.empty() && mCurrentToken->mText != name) { return nullopt; }

        Token returnToken = *mCurrentToken;
        ++mCurrentToken;
        return returnToken;
    }

    optional<Type> Parser::expectType() {
        optional<Token> possibleType = expectIdentifier();
        if (!possibleType) { return nullopt; }

        map<string, Type>::iterator foundType = mTypes.find(possibleType->mText);
        if (foundType == mTypes.end()) { 
            --mCurrentToken;
            return nullopt; 
        }

        return foundType->second;
    }    

    optional<vector<Statement>> Parser::parseFunctionBody() {
        if (!expectOperator("{").has_value()) {
            return nullopt;
        }

        vector<Statement> statements;

        while (!expectOperator("}").has_value()) {
            optional<Statement> statement = expectStatement();
            if (statement.has_value()) {
                statements.push_back(statement.value());
            } 

            if (!expectOperator(";").has_value()) {
                throw runtime_error("Expected ';' at end of statement.");
            }
        }

        return statements;
    }

    optional<Statement> Parser::expectOneValue() {
        optional<Statement> result;
        if (mCurrentToken != mEndToken && mCurrentToken->mType == DOUBLE_LITERAL) {
            Statement doubleLiteralStatement;
            doubleLiteralStatement.mKind = StatementKind::LITERAL;
            doubleLiteralStatement.mName = mCurrentToken->mText;
            doubleLiteralStatement.mType = Type("double", DOUBLE);
            result = doubleLiteralStatement;
            ++mCurrentToken;
        } else if (mCurrentToken != mEndToken && mCurrentToken->mType == INTEGER_LITERAL) {
            Statement integerLiteralStatement;
            integerLiteralStatement.mKind = StatementKind::LITERAL;
            integerLiteralStatement.mName = mCurrentToken->mText;
            integerLiteralStatement.mType = Type("signed integer", INT32);
            result = integerLiteralStatement;
            ++mCurrentToken;  
        } else if (mCurrentToken != mEndToken && mCurrentToken->mType == STRING_LITERAL) {
            Statement stringLiteralStatement;
            stringLiteralStatement.mKind = StatementKind::LITERAL;
            stringLiteralStatement.mName = mCurrentToken->mText;
            stringLiteralStatement.mType = Type("string", UINT8);
            result = stringLiteralStatement;
            ++mCurrentToken;   
        } else if (expectOperator("(").has_value()) {
            result = expectExpression();
            if (!expectOperator(")").has_value()) {
                throw runtime_error("Unbalanced '(' in parenthesized expression.");
            }
        }

        if (!result.has_value()) {
            result = expectFunctionCall();
        }
        
        return result;
    }

    optional<Statement> Parser::expectVariableDeclaration() {
        vector<Token>::iterator startToken = mCurrentToken;

        optional<Type> possibleType = expectType();
        if (!possibleType.has_value()) {
            mCurrentToken = startToken;
            return nullopt;
        }

        optional<Token> possibleVariableName = expectIdentifier();
        if (!possibleVariableName.has_value()) {
            mCurrentToken = startToken;
            return nullopt;
        }

        Statement statement;
        statement.mKind = StatementKind::VARIABLE_DECLARATION;
        statement.mName = possibleVariableName->mText;
        statement.mType = possibleType.value();

        if (expectOperator("=").has_value()) {
            optional<Statement> initialValue = expectExpression();
            if (!initialValue.has_value()) {
                throw runtime_error("Expected initial value to the right of '=' in variable declaration.");
            }

            statement.mParameters.push_back(initialValue.value());
        }

        return statement;
    }

    optional<Statement> Parser::expectFunctionCall() {
        vector<Token>::iterator startToken = mCurrentToken;

        optional<Token> possibleFunctionName = expectIdentifier();
        if (!possibleFunctionName.has_value()) {
            mCurrentToken = startToken;
            return nullopt;
        }

        if (!expectOperator("(").has_value()) {
            mCurrentToken = startToken;
            return nullopt;
        }

        Statement functionCall;
        functionCall.mKind = StatementKind::FUNCTION_CALL;
        functionCall.mName = possibleFunctionName->mText;

        while (!expectOperator(")").has_value()) {
            optional<Statement> parameter = expectExpression();
            if (!parameter.has_value()) {
                throw runtime_error("Expected expression as parameter");
            } 
            functionCall.mParameters.push_back(parameter.value());

            if (expectOperator(")").has_value()) {
                break;
            } 

            if (!expectOperator(",").has_value()) {
                throw runtime_error("Expected ',' to separate parameters.");
            }
        }

        return functionCall;
    }

    optional<Statement> Parser::expectStatement() {
        optional<Statement> result = expectExpression();
        if (!result.has_value()) {
            result = expectVariableDeclaration();
        }

        return result;
    }

    optional<Statement> Parser::expectExpression() {
        optional<Statement> lhs = expectOneValue();
        if (!lhs.has_value()) { return nullopt; }

        while(true) {
            optional<Token> op = expectOperator();
            if (!op.has_value()) { break; }

            int rhsPrecedence = operatorPrecedence(op->mText);
            if (rhsPrecedence == 0) {
                mCurrentToken--;
                return lhs;
            }
            optional<Statement> rhs = expectOneValue();
            if (!rhs.has_value()) {
                mCurrentToken--;
                return lhs;
            }

            Statement * rightmostStatement = findRightmostStatement(&lhs.value(), rhsPrecedence);
            if (rightmostStatement) {
                Statement operatorCall;
                operatorCall.mKind = StatementKind::OPERATOR_CALL;
                operatorCall.mName = op->mText;
                operatorCall.mParameters.push_back(rightmostStatement->mParameters.at(1));
                operatorCall.mParameters.push_back(rhs.value());
                rightmostStatement->mParameters[1] = operatorCall;
            } else {
                Statement operatorCall;
                operatorCall.mKind = StatementKind::OPERATOR_CALL;
                operatorCall.mName = op->mText;
                operatorCall.mParameters.push_back(lhs.value());
                operatorCall.mParameters.push_back(rhs.value());
                lhs = operatorCall;
            }

        }

        return lhs;
    }

    Statement* Parser::findRightmostStatement(Statement *lhs, size_t rhsPrecedence) {
        if (lhs->mKind != StatementKind::OPERATOR_CALL) { return nullptr; }
        if (operatorPrecedence(lhs->mName) >= rhsPrecedence) { return nullptr; }

        Statement * rhs = &lhs->mParameters.at(1);
        if (rhs->mKind != StatementKind::OPERATOR_CALL) { return lhs; }
        rhs = findRightmostStatement(rhs, rhsPrecedence);
        if (rhs == nullptr) { return lhs; }
        return rhs;
    }

    size_t Parser::operatorPrecedence(const string &operatorName) {
        map<string, OperatorEntry>::iterator foundOperator = sOperators.find(operatorName);
        if (foundOperator == sOperators.end()) { return 0; }
        return foundOperator->second.mPrecedence;
    }

    void Parser::debugPrint() {
        for (auto funcPair : mFunctions) {
            funcPair.second.debugPrint();
        }
    }
}