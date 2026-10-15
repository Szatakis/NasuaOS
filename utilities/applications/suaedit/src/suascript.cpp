#include "suascript.hpp"

namespace SuaScript
{
    namespace
    {
        constexpr uint16_t max_tokens = 512;
        constexpr uint16_t max_instructions = 512;
        constexpr uint8_t max_variables = 32;
        constexpr uint8_t max_stack = 64;
        constexpr uint32_t max_steps = 100000;

        enum class TokenKind : uint8_t
        {
            End,
            Identifier,
            Number,
            String,
            Int,
            Bool,
            StringType,
            True,
            False,
            Print,
            If,
            Else,
            While,
            For,
            And,
            Or,
            Not,
            LeftParen,
            RightParen,
            LeftBrace,
            RightBrace,
            Semicolon,
            Assign,
            Plus,
            Minus,
            Star,
            Slash,
            Equal,
            NotEqual,
            Less,
            LessEqual,
            Greater,
            GreaterEqual
        };

        enum class ValueType : uint8_t
        {
            Invalid,
            Integer,
            Boolean,
            String
        };

        struct Token
        {
            TokenKind kind;
            uint16_t start;
            uint16_t length;
            uint16_t line;
            int32_t number;
        };

        enum class Op : uint8_t
        {
            PushInteger,
            PushBoolean,
            PushString,
            Load,
            Store,
            Add,
            Subtract,
            Multiply,
            Divide,
            Negate,
            Equal,
            NotEqual,
            Less,
            LessEqual,
            Greater,
            GreaterEqual,
            And,
            Or,
            Not,
            Jump,
            JumpIfFalse,
            Print
        };

        struct Instruction
        {
            Op op;
            int32_t operand;
        };

        struct Variable
        {
            char name[24];
            ValueType type;
        };

        struct Value
        {
            ValueType type;
            int32_t integer;
            uint16_t token;
        };

        struct Engine
        {
            const char* source;
            uint16_t source_length;
            Output output;
            Token tokens[max_tokens];
            uint16_t token_count;
            uint16_t position;
            Instruction code[max_instructions];
            uint16_t code_count;
            Variable variables[max_variables];
            uint8_t variable_count;
            uint16_t error_line;
            const char* error;
        };

        static Engine engine;

        struct RuntimeValue
        {
            ValueType type;
            int32_t integer;
            uint16_t token;
        };

        void write_text(const char* text)
        {
            if (!engine.output)
            {
                return;
            }
            while (*text)
            {
                engine.output(*text++);
            }
        }

        void write_unsigned(uint32_t value)
        {
            char digits[11];
            uint8_t count = 0;
            do
            {
                digits[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && count < sizeof(digits));
            while (count > 0)
            {
                engine.output(digits[--count]);
            }
        }

        void write_integer(int32_t value)
        {
            if (value < 0)
            {
                engine.output('-');
                write_unsigned(static_cast<uint32_t>(-(static_cast<int64_t>(value))));
            }
            else
            {
                write_unsigned(static_cast<uint32_t>(value));
            }
        }

        void set_error(const char* message, uint16_t line)
        {
            if (!engine.error)
            {
                engine.error = message;
                engine.error_line = line;
            }
        }

        void report_error(const char* prefix)
        {
            write_text(prefix);
            write_text(engine.error ? engine.error : "unknown error");
            write_text(" at line ");
            write_unsigned(engine.error_line);
            write_text("\n");
        }

        bool text_equals(uint16_t start, uint16_t length, const char* text)
        {
            uint16_t index = 0;
            while (index < length && text[index] != '\0' && engine.source[start + index] == text[index])
            {
                ++index;
            }
            return index == length && text[index] == '\0';
        }

        TokenKind keyword_kind(uint16_t start, uint16_t length)
        {
            if (text_equals(start, length, "int")) return TokenKind::Int;
            if (text_equals(start, length, "bool")) return TokenKind::Bool;
            if (text_equals(start, length, "string")) return TokenKind::StringType;
            if (text_equals(start, length, "true")) return TokenKind::True;
            if (text_equals(start, length, "false")) return TokenKind::False;
            if (text_equals(start, length, "print")) return TokenKind::Print;
            if (text_equals(start, length, "if")) return TokenKind::If;
            if (text_equals(start, length, "else")) return TokenKind::Else;
            if (text_equals(start, length, "while")) return TokenKind::While;
            if (text_equals(start, length, "for")) return TokenKind::For;
            if (text_equals(start, length, "and")) return TokenKind::And;
            if (text_equals(start, length, "or")) return TokenKind::Or;
            if (text_equals(start, length, "not")) return TokenKind::Not;
            return TokenKind::Identifier;
        }

        bool add_token(TokenKind kind, uint16_t start, uint16_t length, uint16_t line, int32_t number = 0)
        {
            if (engine.token_count >= max_tokens - 1)
            {
                set_error("too many tokens", line);
                return false;
            }
            engine.tokens[engine.token_count++] = {kind, start, length, line, number};
            return true;
        }

        bool tokenize()
        {
            uint16_t position = 0;
            uint16_t line = 1;
            while (position < engine.source_length)
            {
                const char character = engine.source[position];
                if (character == ' ' || character == '\t' || character == '\r')
                {
                    ++position;
                    continue;
                }
                if (character == '\n')
                {
                    ++line;
                    ++position;
                    continue;
                }
                if (character == '#')
                {
                    while (position < engine.source_length && engine.source[position] != '\n') ++position;
                    continue;
                }
                if (character == '/' && position + 1 < engine.source_length && engine.source[position + 1] == '/')
                {
                    while (position < engine.source_length && engine.source[position] != '\n') ++position;
                    continue;
                }
                if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_')
                {
                    const uint16_t start = position++;
                    while (position < engine.source_length)
                    {
                        const char next = engine.source[position];
                        if (!((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                              (next >= '0' && next <= '9') || next == '_')) break;
                        ++position;
                    }
                    const uint16_t length = position - start;
                    if (!add_token(keyword_kind(start, length), start, length, line)) return false;
                    continue;
                }
                if (character >= '0' && character <= '9')
                {
                    const uint16_t start = position;
                    int64_t number = 0;
                    while (position < engine.source_length && engine.source[position] >= '0' && engine.source[position] <= '9')
                    {
                        number = number * 10 + engine.source[position++] - '0';
                        if (number > 2147483647)
                        {
                            set_error("integer literal is too large", line);
                            return false;
                        }
                    }
                    if (!add_token(TokenKind::Number, start, position - start, line, static_cast<int32_t>(number))) return false;
                    continue;
                }
                if (character == '"')
                {
                    const uint16_t start = ++position;
                    while (position < engine.source_length && engine.source[position] != '"' && engine.source[position] != '\n')
                    {
                        if (engine.source[position] == '\\' && position + 1 < engine.source_length) ++position;
                        ++position;
                    }
                    if (position >= engine.source_length || engine.source[position] != '"')
                    {
                        set_error("unterminated string", line);
                        return false;
                    }
                    if (!add_token(TokenKind::String, start, position - start, line)) return false;
                    ++position;
                    continue;
                }

                TokenKind kind;
                uint16_t length = 1;
                switch (character)
                {
                    case '(': kind = TokenKind::LeftParen; break;
                    case ')': kind = TokenKind::RightParen; break;
                    case '{': kind = TokenKind::LeftBrace; break;
                    case '}': kind = TokenKind::RightBrace; break;
                    case ';': kind = TokenKind::Semicolon; break;
                    case '+': kind = TokenKind::Plus; break;
                    case '-': kind = TokenKind::Minus; break;
                    case '*': kind = TokenKind::Star; break;
                    case '/': kind = TokenKind::Slash; break;
                    case '=':
                        kind = position + 1 < engine.source_length && engine.source[position + 1] == '=' ? TokenKind::Equal : TokenKind::Assign;
                        if (kind == TokenKind::Equal) length = 2;
                        break;
                    case '!':
                        if (position + 1 < engine.source_length && engine.source[position + 1] == '=')
                        {
                            kind = TokenKind::NotEqual;
                            length = 2;
                        }
                        else kind = TokenKind::Not;
                        break;
                    case '<':
                        kind = position + 1 < engine.source_length && engine.source[position + 1] == '=' ? TokenKind::LessEqual : TokenKind::Less;
                        if (kind == TokenKind::LessEqual) length = 2;
                        break;
                    case '>':
                        kind = position + 1 < engine.source_length && engine.source[position + 1] == '=' ? TokenKind::GreaterEqual : TokenKind::Greater;
                        if (kind == TokenKind::GreaterEqual) length = 2;
                        break;
                    default:
                        set_error("unexpected character", line);
                        return false;
                }
                if (!add_token(kind, position, length, line)) return false;
                position = static_cast<uint16_t>(position + length);
            }
            return add_token(TokenKind::End, engine.source_length, 0, line);
        }

        Token& current()
        {
            return engine.tokens[engine.position];
        }

        bool match(TokenKind kind)
        {
            if (current().kind != kind) return false;
            ++engine.position;
            return true;
        }

        bool expect(TokenKind kind, const char* message)
        {
            if (match(kind)) return true;
            set_error(message, current().line);
            return false;
        }

        int emit(Op op, int32_t operand = 0)
        {
            if (engine.code_count >= max_instructions)
            {
                set_error("program is too large", current().line);
                return -1;
            }
            const int index = engine.code_count++;
            engine.code[index] = {op, operand};
            return index;
        }

        void patch(int instruction, int target)
        {
            if (instruction >= 0 && instruction < engine.code_count)
            {
                engine.code[instruction].operand = target;
            }
        }

        int find_variable(const Token& token)
        {
            for (uint8_t variable = 0; variable < engine.variable_count; ++variable)
            {
                uint16_t index = 0;
                while (index < token.length && engine.variables[variable].name[index] != '\0' &&
                       engine.variables[variable].name[index] == engine.source[token.start + index]) ++index;
                if (index == token.length && engine.variables[variable].name[index] == '\0') return variable;
            }
            return -1;
        }

        int declare_variable(const Token& token, ValueType type)
        {
            if (token.length >= sizeof(engine.variables[0].name))
            {
                set_error("variable name is too long", token.line);
                return -1;
            }
            if (find_variable(token) >= 0)
            {
                set_error("variable is already declared", token.line);
                return -1;
            }
            if (engine.variable_count >= max_variables)
            {
                set_error("too many variables", token.line);
                return -1;
            }
            const int slot = engine.variable_count++;
            for (uint16_t index = 0; index < token.length; ++index)
            {
                engine.variables[slot].name[index] = engine.source[token.start + index];
            }
            engine.variables[slot].name[token.length] = '\0';
            engine.variables[slot].type = type;
            return slot;
        }

        ValueType parse_expression();

        ValueType parse_primary()
        {
            if (match(TokenKind::Number))
            {
                emit(Op::PushInteger, engine.tokens[engine.position - 1].number);
                return ValueType::Integer;
            }
            if (match(TokenKind::String))
            {
                emit(Op::PushString, engine.position - 1);
                return ValueType::String;
            }
            if (match(TokenKind::True) || match(TokenKind::False))
            {
                const bool value = engine.tokens[engine.position - 1].kind == TokenKind::True;
                emit(Op::PushBoolean, value ? 1 : 0);
                return ValueType::Boolean;
            }
            if (match(TokenKind::Identifier))
            {
                const Token& token = engine.tokens[engine.position - 1];
                const int variable = find_variable(token);
                if (variable < 0)
                {
                    set_error("unknown variable", token.line);
                    return ValueType::Invalid;
                }
                emit(Op::Load, variable);
                return engine.variables[variable].type;
            }
            if (match(TokenKind::LeftParen))
            {
                const ValueType type = parse_expression();
                expect(TokenKind::RightParen, "expected ')' after expression");
                return type;
            }
            set_error("expected an expression", current().line);
            return ValueType::Invalid;
        }

        ValueType parse_unary()
        {
            if (match(TokenKind::Minus))
            {
                const ValueType type = parse_unary();
                if (type != ValueType::Integer) set_error("unary '-' requires int", current().line);
                emit(Op::Negate);
                return ValueType::Integer;
            }
            if (match(TokenKind::Not))
            {
                const ValueType type = parse_unary();
                if (type != ValueType::Boolean) set_error("not requires bool", current().line);
                emit(Op::Not);
                return ValueType::Boolean;
            }
            return parse_primary();
        }

        ValueType parse_factor()
        {
            ValueType type = parse_unary();
            while (current().kind == TokenKind::Star || current().kind == TokenKind::Slash)
            {
                const TokenKind operation = engine.tokens[engine.position++].kind;
                const ValueType right = parse_unary();
                if (type != ValueType::Integer || right != ValueType::Integer) set_error("arithmetic requires int", current().line);
                emit(operation == TokenKind::Star ? Op::Multiply : Op::Divide);
                type = ValueType::Integer;
            }
            return type;
        }

        ValueType parse_term()
        {
            ValueType type = parse_factor();
            while (current().kind == TokenKind::Plus || current().kind == TokenKind::Minus)
            {
                const TokenKind operation = engine.tokens[engine.position++].kind;
                const ValueType right = parse_factor();
                if (type != ValueType::Integer || right != ValueType::Integer) set_error("arithmetic requires int", current().line);
                emit(operation == TokenKind::Plus ? Op::Add : Op::Subtract);
                type = ValueType::Integer;
            }
            return type;
        }

        ValueType parse_comparison()
        {
            ValueType type = parse_term();
            while (current().kind == TokenKind::Less || current().kind == TokenKind::LessEqual ||
                   current().kind == TokenKind::Greater || current().kind == TokenKind::GreaterEqual)
            {
                const TokenKind operation = engine.tokens[engine.position++].kind;
                const ValueType right = parse_term();
                if (type != ValueType::Integer || right != ValueType::Integer) set_error("comparison requires int", current().line);
                Op bytecode = Op::Less;
                if (operation == TokenKind::LessEqual) bytecode = Op::LessEqual;
                else if (operation == TokenKind::Greater) bytecode = Op::Greater;
                else if (operation == TokenKind::GreaterEqual) bytecode = Op::GreaterEqual;
                emit(bytecode);
                type = ValueType::Boolean;
            }
            return type;
        }

        ValueType parse_equality()
        {
            ValueType type = parse_comparison();
            while (current().kind == TokenKind::Equal || current().kind == TokenKind::NotEqual)
            {
                const TokenKind operation = engine.tokens[engine.position++].kind;
                const ValueType right = parse_comparison();
                if (type != right) set_error("equality operands must have the same type", current().line);
                emit(operation == TokenKind::Equal ? Op::Equal : Op::NotEqual);
                type = ValueType::Boolean;
            }
            return type;
        }

        ValueType parse_and()
        {
            ValueType type = parse_equality();
            while (match(TokenKind::And))
            {
                const ValueType right = parse_equality();
                if (type != ValueType::Boolean || right != ValueType::Boolean) set_error("and requires bool", current().line);
                emit(Op::And);
                type = ValueType::Boolean;
            }
            return type;
        }

        ValueType parse_expression()
        {
            ValueType type = parse_and();
            while (match(TokenKind::Or))
            {
                const ValueType right = parse_and();
                if (type != ValueType::Boolean || right != ValueType::Boolean) set_error("or requires bool", current().line);
                emit(Op::Or);
                type = ValueType::Boolean;
            }
            return type;
        }

        bool parse_declaration(bool require_semicolon)
        {
            const Token type_token = engine.tokens[engine.position++];
            ValueType declared_type = ValueType::Integer;
            if (type_token.kind == TokenKind::Bool) declared_type = ValueType::Boolean;
            else if (type_token.kind == TokenKind::StringType) declared_type = ValueType::String;
            if (!expect(TokenKind::Identifier, "expected a variable name")) return false;
            const Token name = engine.tokens[engine.position - 1];
            const int variable = declare_variable(name, declared_type);
            if (variable < 0 || !expect(TokenKind::Assign, "expected '=' after variable name")) return false;
            const ValueType value_type = parse_expression();
            if (value_type != declared_type) set_error("assigned value does not match variable type", name.line);
            emit(Op::Store, variable);
            if (require_semicolon)
            {
                if (!expect(TokenKind::Semicolon, "expected ';' in for header")) return false;
            }
            else
            {
                match(TokenKind::Semicolon);
            }
            return engine.error == nullptr;
        }

        bool parse_assignment(bool require_semicolon)
        {
            if (!expect(TokenKind::Identifier, "expected a variable name")) return false;
            const Token name = engine.tokens[engine.position - 1];
            const int variable = find_variable(name);
            if (variable < 0)
            {
                set_error("unknown variable", name.line);
                return false;
            }
            if (!expect(TokenKind::Assign, "expected '=' in assignment")) return false;
            const ValueType value_type = parse_expression();
            if (value_type != engine.variables[variable].type) set_error("assigned value does not match variable type", name.line);
            emit(Op::Store, variable);
            if (require_semicolon)
            {
                if (!expect(TokenKind::Semicolon, "expected ';' in for header")) return false;
            }
            else
            {
                match(TokenKind::Semicolon);
            }
            return engine.error == nullptr;
        }

        bool parse_statement();

        bool parse_block()
        {
            if (!expect(TokenKind::LeftBrace, "expected '{' to begin block")) return false;
            while (current().kind != TokenKind::RightBrace && current().kind != TokenKind::End && !engine.error)
            {
                const uint16_t previous = engine.position;
                if (!parse_statement()) return false;
                if (engine.position == previous)
                {
                    set_error("parser made no progress", current().line);
                    return false;
                }
            }
            return expect(TokenKind::RightBrace, "expected '}' to end block");
        }

        bool parse_if_tail()
        {
            if (!expect(TokenKind::LeftParen, "expected '(' after if")) return false;
            const ValueType condition = parse_expression();
            if (condition != ValueType::Boolean) set_error("if condition must be bool", current().line);
            if (!expect(TokenKind::RightParen, "expected ')' after condition")) return false;
            const int false_jump = emit(Op::JumpIfFalse);
            if (!parse_block()) return false;
            if (match(TokenKind::Else))
            {
                const int end_jump = emit(Op::Jump);
                patch(false_jump, engine.code_count);
                if (match(TokenKind::If))
                {
                    if (!parse_if_tail()) return false;
                }
                else if (!parse_block())
                {
                    return false;
                }
                patch(end_jump, engine.code_count);
            }
            else
            {
                patch(false_jump, engine.code_count);
            }
            return engine.error == nullptr;
        }

        bool parse_while()
        {
            ++engine.position;
            const int condition_start = engine.code_count;
            if (!expect(TokenKind::LeftParen, "expected '(' after while")) return false;
            const ValueType condition = parse_expression();
            if (condition != ValueType::Boolean) set_error("while condition must be bool", current().line);
            if (!expect(TokenKind::RightParen, "expected ')' after condition")) return false;
            const int exit_jump = emit(Op::JumpIfFalse);
            if (!parse_block()) return false;
            emit(Op::Jump, condition_start);
            patch(exit_jump, engine.code_count);
            return engine.error == nullptr;
        }

        bool parse_for()
        {
            ++engine.position;
            if (!expect(TokenKind::LeftParen, "expected '(' after for")) return false;
            if (current().kind != TokenKind::Int && current().kind != TokenKind::Bool && current().kind != TokenKind::StringType)
            {
                set_error("for initializer must declare a typed variable", current().line);
                return false;
            }
            if (!parse_declaration(true)) return false;

            const int condition_start = engine.code_count;
            const ValueType condition = parse_expression();
            if (condition != ValueType::Boolean) set_error("for condition must be bool", current().line);
            if (!expect(TokenKind::Semicolon, "expected ';' before for increment")) return false;
            const int exit_jump = emit(Op::JumpIfFalse);
            const int body_jump = emit(Op::Jump);
            const int increment_start = engine.code_count;
            if (!parse_assignment(false) || !expect(TokenKind::RightParen, "expected ')' after for header")) return false;
            emit(Op::Jump, condition_start);
            patch(body_jump, engine.code_count);
            if (!parse_block()) return false;
            emit(Op::Jump, increment_start);
            patch(exit_jump, engine.code_count);
            return engine.error == nullptr;
        }

        bool parse_statement()
        {
            if (current().kind == TokenKind::Int || current().kind == TokenKind::Bool || current().kind == TokenKind::StringType)
            {
                return parse_declaration(false);
            }
            if (match(TokenKind::Print))
            {
                if (!expect(TokenKind::LeftParen, "expected '(' after print")) return false;
                if (parse_expression() == ValueType::Invalid) return false;
                if (!expect(TokenKind::RightParen, "expected ')' after print value")) return false;
                emit(Op::Print);
                match(TokenKind::Semicolon);
                return engine.error == nullptr;
            }
            if (current().kind == TokenKind::If)
            {
                ++engine.position;
                return parse_if_tail();
            }
            if (current().kind == TokenKind::While) return parse_while();
            if (current().kind == TokenKind::For) return parse_for();
            if (current().kind == TokenKind::Identifier) return parse_assignment(false);
            set_error("expected a statement", current().line);
            return false;
        }

        bool compile()
        {
            while (current().kind != TokenKind::End && !engine.error)
            {
                const uint16_t previous = engine.position;
                if (!parse_statement()) return false;
                if (engine.position == previous)
                {
                    set_error("parser made no progress", current().line);
                    return false;
                }
            }
            return engine.error == nullptr;
        }

        bool push(RuntimeValue* stack, uint8_t& stack_size, RuntimeValue value)
        {
            if (stack_size >= max_stack) return false;
            stack[stack_size++] = value;
            return true;
        }

        bool values_equal(const RuntimeValue& left, const RuntimeValue& right)
        {
            if (left.type != right.type)
            {
                return false;
            }
            if (left.type != ValueType::String)
            {
                return left.integer == right.integer;
            }
            const Token& left_string = engine.tokens[left.token];
            const Token& right_string = engine.tokens[right.token];
            if (left_string.length != right_string.length)
            {
                return false;
            }
            for (uint16_t index = 0; index < left_string.length; ++index)
            {
                if (engine.source[left_string.start + index] != engine.source[right_string.start + index])
                {
                    return false;
                }
            }
            return true;
        }

        bool run()
        {
            RuntimeValue variables[max_variables];
            RuntimeValue stack[max_stack];
            for (uint8_t index = 0; index < max_variables; ++index)
            {
                variables[index] = {ValueType::Invalid, 0, 0};
            }
            uint8_t stack_size = 0;
            uint16_t pc = 0;
            uint32_t steps = 0;
            while (pc < engine.code_count)
            {
                if (++steps > max_steps)
                {
                    write_text("Runtime error: instruction limit exceeded\n");
                    return false;
                }
                const Instruction instruction = engine.code[pc++];
                RuntimeValue left = {};
                RuntimeValue right = {};
                RuntimeValue result = {};
                switch (instruction.op)
                {
                    case Op::PushInteger:
                        if (!push(stack, stack_size, {ValueType::Integer, instruction.operand, 0})) goto stack_error;
                        break;
                    case Op::PushBoolean:
                        if (!push(stack, stack_size, {ValueType::Boolean, instruction.operand, 0})) goto stack_error;
                        break;
                    case Op::PushString:
                        if (!push(stack, stack_size, {ValueType::String, 0, static_cast<uint16_t>(instruction.operand)})) goto stack_error;
                        break;
                    case Op::Load:
                        if (instruction.operand < 0 || instruction.operand >= engine.variable_count ||
                            !push(stack, stack_size, variables[instruction.operand])) goto stack_error;
                        break;
                    case Op::Store:
                        if (stack_size == 0 || instruction.operand < 0 || instruction.operand >= engine.variable_count) goto stack_error;
                        variables[instruction.operand] = stack[--stack_size];
                        break;
                    case Op::Negate:
                    case Op::Not:
                        if (stack_size == 0) goto stack_error;
                        left = stack[--stack_size];
                        result.type = instruction.op == Op::Negate ? ValueType::Integer : ValueType::Boolean;
                        result.integer = instruction.op == Op::Negate ? static_cast<int32_t>(-static_cast<int64_t>(left.integer)) : !left.integer;
                        if (!push(stack, stack_size, result)) goto stack_error;
                        break;
                    case Op::Print:
                        if (stack_size == 0) goto stack_error;
                        left = stack[--stack_size];
                        if (left.type == ValueType::String)
                        {
                            const Token& token = engine.tokens[left.token];
                            for (uint16_t index = 0; index < token.length; ++index)
                            {
                                char character = engine.source[token.start + index];
                                if (character == '\\' && index + 1 < token.length)
                                {
                                    const char escaped = engine.source[token.start + ++index];
                                    if (escaped == 'n') character = '\n';
                                    else if (escaped == 't') character = '\t';
                                    else character = escaped;
                                }
                                engine.output(character);
                            }
                        }
                        else if (left.type == ValueType::Boolean)
                        {
                            write_text(left.integer ? "true" : "false");
                        }
                        else
                        {
                            write_integer(left.integer);
                        }
                        engine.output('\n');
                        break;
                    case Op::Jump:
                        if (instruction.operand < 0 || instruction.operand > engine.code_count) goto stack_error;
                        pc = static_cast<uint16_t>(instruction.operand);
                        break;
                    case Op::JumpIfFalse:
                        if (stack_size == 0) goto stack_error;
                        left = stack[--stack_size];
                        if (!left.integer)
                        {
                            if (instruction.operand < 0 || instruction.operand > engine.code_count) goto stack_error;
                            pc = static_cast<uint16_t>(instruction.operand);
                        }
                        break;
                    default:
                        if (stack_size < 2) goto stack_error;
                        right = stack[--stack_size];
                        left = stack[--stack_size];
                        result.type = ValueType::Integer;
                        switch (instruction.op)
                        {
                            case Op::Add: result.integer = static_cast<int32_t>(static_cast<int64_t>(left.integer) + right.integer); break;
                            case Op::Subtract: result.integer = static_cast<int32_t>(static_cast<int64_t>(left.integer) - right.integer); break;
                            case Op::Multiply: result.integer = static_cast<int32_t>(static_cast<int64_t>(left.integer) * right.integer); break;
                            case Op::Divide:
                                if (right.integer == 0)
                                {
                                    write_text("Runtime error: division by zero\n");
                                    return false;
                                }
                                result.integer = left.integer / right.integer;
                                break;
                            case Op::Equal: result.type = ValueType::Boolean; result.integer = values_equal(left, right); break;
                            case Op::NotEqual: result.type = ValueType::Boolean; result.integer = !values_equal(left, right); break;
                            case Op::Less: result.type = ValueType::Boolean; result.integer = left.integer < right.integer; break;
                            case Op::LessEqual: result.type = ValueType::Boolean; result.integer = left.integer <= right.integer; break;
                            case Op::Greater: result.type = ValueType::Boolean; result.integer = left.integer > right.integer; break;
                            case Op::GreaterEqual: result.type = ValueType::Boolean; result.integer = left.integer >= right.integer; break;
                            case Op::And: result.type = ValueType::Boolean; result.integer = left.integer && right.integer; break;
                            case Op::Or: result.type = ValueType::Boolean; result.integer = left.integer || right.integer; break;
                            default: goto stack_error;
                        }
                        if (!push(stack, stack_size, result)) goto stack_error;
                        break;
                }
            }
            return true;

        stack_error:
            write_text("Runtime error: invalid program state\n");
            return false;
        }
    }

    bool compile_and_run(const char* source, uint16_t length, Output output)
    {
        engine.token_count = 0;
        engine.position = 0;
        engine.code_count = 0;
        engine.variable_count = 0;
        engine.error_line = 0;
        engine.error = nullptr;
        engine.source = source;
        engine.source_length = length;
        engine.output = output;
        if (!source || !output)
        {
            write_text("Compile error: missing source or output\n");
            return false;
        }
        if (!tokenize() || !compile())
        {
            report_error("Compile error: ");
            return false;
        }
        return run();
    }
}