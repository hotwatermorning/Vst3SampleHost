#pragma once

NS_HWM_BEGIN

//! a class represents either success or failure
//! if succeeded, is_right() and operator bool() returns true.
template<class Left, class Right>
class Either
{
public:
    template<class T>
    Either(T &&data) : data_(std::forward<T>(data)) {}
    
    bool is_right() const { return data_.index() == 1; }
    
    explicit operator bool() const { return is_right(); }
    
    Left & left() {
        if(auto p = std::get_if<Left>(&data_)) {
            return *p;
        } else {
            throw std::runtime_error("Left is unavailable");
            return *(Left *)nullptr;
        }
    }
    
    Left const & left() const {
        if(auto p = std::get_if<Left>(&data_)) {
            return *p;
        } else {
            throw std::runtime_error("Left is unavailable");
            return *(Left *)nullptr;
        }
    }
    Right & right() {
        if(auto p = std::get_if<Right>(&data_)) {
            return *p;
        } else {
            throw std::runtime_error("Left is unavailable");
            return *(Right *)nullptr;
        }
    }
    Right & right() const {
        if(auto p = std::get_if<Right>(&data_)) {
            return *p;
        } else {
            throw std::runtime_error("Left is unavailable");
            return *(Right *)nullptr;
        }
    }
    
    template<class F>
    auto visit(F f) { return std::visit(f, data_); }
    
    template<class F>
    auto visit(F f) const { return std::visit(f, data_); }
    
private:
    std::variant<Left, Right> data_;
};

NS_HWM_END
