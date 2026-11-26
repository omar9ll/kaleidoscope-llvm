#pragma once
#include <cctype>
#include <istream>
#include <string>

// Tokens are either one of these negative codes or a plain ASCII character like '(' or '+'.
enum Token : int {
    tok_eof = -1,
    tok_def = -2, tok_extern = -3,
    tok_identifier = -4, tok_number = -5,
    tok_if = -6, tok_then = -7, tok_else = -8,
    tok_for = -9, tok_in = -10,
};

class Lexer {
public:
    explicit Lexer(std::istream& in) : in_(in) {}

    int next() { cur_ = gettok(); return cur_; }
    int cur() const { return cur_; }
    const std::string& identifier() const { return ident_; }
    double number() const { return num_; }

private:
    std::istream& in_;
    int last_ = ' ';
    int cur_ = 0;
    std::string ident_;
    double num_ = 0;

    int gettok() {
        while (std::isspace(last_)) last_ = in_.get();

        if (std::isalpha(last_)) {                       // identifier or keyword
            ident_ = static_cast<char>(last_);
            while (std::isalnum(last_ = in_.get())) ident_ += static_cast<char>(last_);
            if (ident_ == "def") return tok_def;
            if (ident_ == "extern") return tok_extern;
            if (ident_ == "if") return tok_if;
            if (ident_ == "then") return tok_then;
            if (ident_ == "else") return tok_else;
            if (ident_ == "for") return tok_for;
            if (ident_ == "in") return tok_in;
            return tok_identifier;
        }
        if (std::isdigit(last_) || last_ == '.') {       // number
            std::string s;
            do { s += static_cast<char>(last_); last_ = in_.get(); } while (std::isdigit(last_) || last_ == '.');
            num_ = std::strtod(s.c_str(), nullptr);
            return tok_number;
        }
        if (last_ == '#') {                              // comment
            do last_ = in_.get(); while (last_ != EOF && last_ != '\n' && last_ != '\r');
            if (last_ != EOF) return gettok();
        }
        if (last_ == EOF) return tok_eof;
        int c = last_;
        last_ = in_.get();
        return c;
    }
};
