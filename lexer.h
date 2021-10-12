#pragma once

#include <iosfwd>
#include <optional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <algorithm>
#include <typeinfo>

namespace parse {

namespace token_type {

struct Number {  // Лексема «число»
    int value;   // число
};

struct Id { // Лексема «идентификатор»
    std::string value;  // Имя идентификатора
};

struct Char { // Лексема «символ»
    char value;  // код символа
};

struct String { // Лексема «строковая константа»
    std::string value;
};

struct Class {};    // Лексема «class»
struct Return {};   // Лексема «return»
struct If {};       // Лексема «if»
struct Else {};     // Лексема «else»
struct Def {};      // Лексема «def»
struct Newline {};  // Лексема «конец строки»
struct Print {};    // Лексема «print»
struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};  // Лексема «уменьшение отступа»
struct Eof {};     // Лексема «конец файла»
struct And {};     // Лексема «and»
struct Or {};      // Лексема «or»
struct Not {};     // Лексема «not»
struct Eq {};      // Лексема «==»
struct NotEq {};   // Лексема «!=»
struct LessOrEq {};     // Лексема «<=»
struct GreaterOrEq {};  // Лексема «>=»
struct None {};         // Лексема «None»
struct True {};         // Лексема «True»
struct False {};        // Лексема «False»

}  // namespace token_type

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const {
        using namespace std::literals;
        const Token& token = this->CurrentToken();
        if (!std::holds_alternative<T>(token))
            throw LexerError("Invalid expectation"s);
        return std::get<T>(token);
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U& value) const {
        using namespace std::literals;
        const Token& token = this->CurrentToken();
        if (!std::holds_alternative<T>(token) ||
            (std::holds_alternative<T>(token) && std::get<T>(token).value != value))
            throw LexerError("Invalid expectation"s);
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext() {
        using namespace std::literals;
        this->NextToken();
        return Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U& value) {
        using namespace std::literals;
        this->NextToken();
        Expect<T, U>(value);
    }

private:
    std::vector<Token> tokens_list_;
    std::istream& input_;
    std::string tmp_string;
    size_t current_token_ = 0;
    bool eof = false;
    int previous_indent_ = 0;

    // Читает строку из потока, разбивает ее на токены
    // и добавляет их в вектор tokens_list_
    bool ParseInputStream_();

    // Находит первое появление символа # в строке,
    // не ограниченного кавычками
    int FindComment_(const std::string& word);

    // Является ли char c символом названия переменной
    bool IsVariableChar_(char c);

    // Содержит ли строка название переменной
    bool IsVariableName_(const std::string& word);

    // Является ли char c цифрой
    bool IsDigit_(char c);

    // Содержит ли строка число
    bool IsNumber_(const std::string& word);

    // Является ли char c отдельным символом
    bool IsOperationChar_(char c);

    // Содержит ли строка операцию
    bool IsOperatinString_(const std::string& word);

    // Если строка пустая или содержит только пробелы
    bool IsLineEmpty_(const std::string& word);

    // Обработчик escape-последовательностей для строки
    std::string EscSeqHandler_(const std::string& word);

    // Переводит любую строку в токен
    void AppendTokensList_(const std::string& word);

    // Переводит строку с ключевым словом в токен
    std::optional<Token> KeyWordToToken_(const std::string& word);

    // Переводит строку с операцией в токен
    std::optional<Token> OperationToToken_(const std::string& word);

};

}  // namespace parse