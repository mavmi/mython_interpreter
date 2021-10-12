#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

ostream& operator<<(ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Lexer::Lexer(istream& input)
    : input_(input) {
    ParseInputStream_();
}
// Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
const Token& Lexer::CurrentToken() const {
    return tokens_list_.at(current_token_);
}

// Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
Token Lexer::NextToken() {
    current_token_++;
    if (current_token_ >= tokens_list_.size() && !ParseInputStream_()){
        current_token_--;
        return tokens_list_.back();
    }
    return tokens_list_.at(current_token_);
}

bool Lexer::ParseInputStream_(){
    string line;
    size_t line_len;
    size_t line_iter = 0;
    char c;
    char quote;
    int indent = 0;
    int comment;

    if (!getline(input_, line)){
        if (!eof){
            eof = true;
            for (int in = 0; in < previous_indent_; ++in){
                tokens_list_.push_back(token_type::Dedent());
            }
            tokens_list_.push_back(token_type::Eof());
            return true;
        }
        return false;
    }
    
    comment = FindComment_(line);
    if (comment != -1){
        line = line.substr(0, comment);
    }

    if (IsLineEmpty_(line)){
        return ParseInputStream_();
    }

    // Проходимся по пробелам в начале строки
    // и создаем токены отступа
    while(line.at(line_iter) == ' '){
        indent += 1;
        line_iter++;
    }
    indent /= 2;
    if (indent != previous_indent_){
        for (int in = 0; in < abs(indent - previous_indent_); ++in){
            if (indent > previous_indent_)
                tokens_list_.push_back(token_type::Indent());
            else
                tokens_list_.push_back(token_type::Dedent());
        }
    }
    previous_indent_ = indent;

    // Пробегаем до конца строки и делим ее на слова
    line_len = line.length();
    while (line_iter < line_len){
        c = line.at(line_iter);
        if (c == ' '){
            line_iter++;
            continue;
        }

        // Если escape-последовательность
        if (c == '\\'){
            tokens_list_.push_back(token_type::Char{'\\'});
            if (line_iter < line_len - 1){
                char escaped_char = line.at(++line_iter);
                switch (escaped_char) {
                    case 'n':
                        tokens_list_.push_back(token_type::Char{'n'});
                        break;
                    case 't':
                        tokens_list_.push_back(token_type::Char{'t'});
                        break;
                    case 'r':
                        tokens_list_.push_back(token_type::Char{'r'});
                        break;
                    case '"':
                        tokens_list_.push_back(token_type::Char{'"'});
                        break;
                    case '\\':
                        tokens_list_.push_back(token_type::Char{'\\'});
                        break;
                    default:
                        line_iter--;
                }
            }
            line_iter++;
            continue;
        }

        // операция или набор символов
        if (IsOperationChar_(c)){
            tmp_string = "";
            while (line_iter < line_len && IsOperationChar_(c) && c != ' '){
                tmp_string += c;
                if (++line_iter < line_len)    
                    c = line.at(line_iter);
            }
            AppendTokensList_(tmp_string);
            continue;
        }

        // название переменной
        if (IsVariableChar_(c)){
            tmp_string = "";
            while (line_iter < line_len && (IsVariableChar_(c) || IsDigit_(c)) && c != ' '){
                tmp_string += c;
                if (++line_iter < line_len)    
                    c = line.at(line_iter);
            }
            AppendTokensList_(tmp_string);
            continue;
        }

        // строка
        if (c == '\'' || c == '"'){
            quote = c;
            size_t begin = line_iter + 1;
            size_t pos = 0;
            while (1){
                pos = line.find(quote, begin);
                if (pos == string::npos || (line.at(pos - 1) != '\\' && line.at(pos) == quote))
                    break;
                begin = pos + 1;
            }
            tmp_string = line.substr(line_iter, pos - line_iter + 1);
            line_iter += pos - line_iter + 1;
            AppendTokensList_(tmp_string);
            continue;
        }

        // число
        if (IsDigit_(c)){
            tmp_string = "";
            while (line_iter < line_len && IsDigit_(c) && c != ' '){
                tmp_string += c;
                if (++line_iter < line_len)    
                    c = line.at(line_iter);
            }
            AppendTokensList_(tmp_string);
            continue;
        }
    }

    tokens_list_.push_back(token_type::Newline());
    return true;
}

int Lexer::FindComment_(const string& word){
    char c;
    bool single_quote = false;
    bool double_quote = false;

    for (size_t i = 0; i < word.length(); ++i){
        c = word.at(i);
        if (c == '\'' && !double_quote)
            single_quote = !single_quote;
        else if (c == '\"' && !single_quote)
            double_quote = !double_quote;
        else if (c == '#' && !single_quote && !double_quote){
            return i;
        }
    }
    return -1;
}

bool Lexer::IsVariableChar_(char c){
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
        return true;
    return false;
}

bool Lexer::IsVariableName_(const string& word){
    if (!(IsVariableChar_(word.at(0)) && !IsDigit_(word.at(0))))
        return false;
    for (size_t i = 1; i < word.length(); ++i){
        if (!IsVariableChar_(word.at(i)))
            return false;
    }
    return true;
}

bool Lexer::IsDigit_(char c){
    if (c >= '0' && c <= '9')
        return true;
    return false;
}

bool Lexer::IsNumber_(const string& word){
    for (char c : word){
        if (!IsDigit_(c))
            return false;
    }
    return true;
}

bool Lexer::IsOperationChar_(char c){
    if (c == '.' || c == ',' || c == '(' || c == ')' ||
        c == '>' || c == '<' || c == ':' || c == '=' ||
        c == '+' || c == '-' || c == '*' || c == '/' ||
        c == '!' || c == '?')
        return true;
    return false;
}

bool Lexer::IsOperatinString_(const string& word){
    for (char c : word){
        if (!IsOperationChar_(c))
            return false;
    }
    return true;
}

bool Lexer::IsLineEmpty_(const string& word){
    if (word.length() == 0)
        return true;
    for (size_t i = 0; i < word.length(); ++i){
        if (word.at(i) != ' ')
            return false;
    }
    return true;
}	

string Lexer::EscSeqHandler_(const string& word){
    size_t word_len;
    char c, escaped_char;
    string result_str;

    word_len = word.length();
    for (size_t i = 0; i < word_len; ++i){
        c = word.at(i);
        if (c == '\\'){
            if (i == word_len - 1){
                throw LexerError("String parsing error"s);
            }
            escaped_char = word.at(++i);
            switch (escaped_char) {
                case 'n':
                    result_str += '\n';
                    break;
                case 't':
                    result_str += '\t';
                    break;
                case 'r':
                    result_str += '\r';
                    break;
                case '"':
                    result_str += '"';
                    break;
                case '\'':
                    result_str += '\'';
                    break;
                case '\\':
                    result_str += '\\';
                    break;
                default:
                    throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
            }
        } else if (c == '\n' || c == '\r') {
                throw LexerError("Unexpected end of line"s);
        } else {
            result_str += c;
        }
    }
    return result_str;
} 

void Lexer::AppendTokensList_(const string& word){
    optional<Token> keyword_token;
    if ((keyword_token = KeyWordToToken_(word))){ // ключевое слово
        tokens_list_.push_back(keyword_token.value());
        return;
    }
    optional<Token> operation_token;
    if ((operation_token = OperationToToken_(word))){ // операция
        tokens_list_.push_back(operation_token.value());
        return;
    }
    if (IsOperatinString_(word)){ // набор символов
        for (char c : word){
            token_type::Char char_token;
            char_token.value = c;
            tokens_list_.push_back(char_token);
        }
        return;
    }
    if (IsNumber_(word)){ // число
        token_type::Number number_token;
        number_token.value = stoi(word);
        tokens_list_.push_back(number_token);
        return;
    }
    if (IsVariableName_(word)){ // переменная (id)
        token_type::Id id_token;
        id_token.value = word;
        tokens_list_.push_back(id_token);
        return;
    }
    if (word.at(0) == '\'' || word.at(0) == '\"'){ // строка
        token_type::String str_token;
        str_token.value = EscSeqHandler_(word.substr(1, word.length() - 2));
        tokens_list_.push_back(str_token);  
        return;
    }
    throw LexerError("invalid input");
}

optional<Token> Lexer::KeyWordToToken_(const string& word){
    if (word == "class"s)
        return token_type::Class();
    else if (word == "return"s)
        return token_type::Return();
    else if (word == "if"s)
        return token_type::If();
    else if (word == "else"s)
        return token_type::Else();
    else if (word == "def"s)
        return token_type::Def();
    else if (word == "print"s)
        return token_type::Print();
    else if (word == "and"s)
        return token_type::And();
    else if (word == "or"s)
        return token_type::Or();
    else if (word == "None"s)
        return token_type::None();
    else if (word == "True"s)
        return token_type::True();
    else if (word == "False"s)
        return token_type::False();
    else if (word == "not"s)
        return token_type::Not();
    return nullopt;
}

optional<Token> Lexer::OperationToToken_(const string& word){
    if (word == "!=")
        return token_type::NotEq();
    else if (word == "==")
        return token_type::Eq();
    else if (word == ">=")
        return token_type::GreaterOrEq();
    else if (word == "<=")
        return token_type::LessOrEq();
    return nullopt;
}

}  // namespace parse