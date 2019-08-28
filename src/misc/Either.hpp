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
        auto p = std::get_if<Left>(&data_);
        if(!p) {
            throw std::runtime_error("Left is unavailable");
        }

        return *p;
    }
    
    Left const & left() const {
        auto p = std::get_if<Left>(&data_);
        if(!p) {
            throw std::runtime_error("Left is unavailable");
        }
        
        return *p;
    }
    
    Right & right() {
        auto p = std::get_if<Right>(&data_);
        if(!p) {
            throw std::runtime_error("Right is unavailable");
        }
        
        return *p;
    }
    Right & right() const {
        auto p = std::get_if<Right>(&data_);
        if(!p) {
            throw std::runtime_error("Right is unavailable");
        }
        
        return *p;
    }
    
    template<class F>
    auto visit(F f) { return std::visit(f, data_); }
    
    template<class F>
    auto visit(F f) const { return std::visit(f, data_); }
    
private:
    std::variant<Left, Right> data_;
};

NS_HWM_END
