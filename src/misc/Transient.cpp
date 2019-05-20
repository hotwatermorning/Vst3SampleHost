#include "Transient.hpp"
#include "MathUtil.hpp"

NS_HWM_BEGIN

namespace {
    static constexpr double kTolerance = 1E-10;
}

Transient::Transient(double sample_rate,
                     UInt32 duration_in_msec,
                     double min_db,
                     double max_db)
:   amount_(log10(2) * 20.0 / (duration_in_msec / 1000.0 * sample_rate))
,   min_db_(min_db)
,   max_db_(max_db)
{}

Transient::Transient()
:   Transient(44100.0, 50, -48, 0.0)
{}

Transient::Transient(Transient &&rhs)
{
    amount_ = rhs.amount_;
    min_db_ = rhs.min_db_;
    max_db_ = rhs.max_db_;
    current_db_ = rhs.current_db_;
    target_db_ = rhs.target_db_.load();
}

Transient & Transient::operator=(Transient &&rhs)
{
    amount_ = rhs.amount_;
    min_db_ = rhs.min_db_;
    max_db_ = rhs.max_db_;
    current_db_ = rhs.current_db_;
    target_db_ = rhs.target_db_.load();
    
    return *this;
}

//! 指定したstepが経過したあとtransient値を計算して設定する
void Transient::update_transient(Int32 step)
{
    assert(step >= 1);
    
    auto const goal = target_db_.load();
    if(fabs(current_db_ - goal) < kTolerance) {
        current_db_ = goal;
    }
    
    if(current_db_ < goal) {
        current_db_ = std::min<double>(current_db_ + amount_ * step, goal);
    } else {
        current_db_ = std::max<double>(current_db_ - amount_ * step, goal);
    }
}

double Transient::get_current_transient_db() const
{
    return current_db_;
}

double Transient::get_current_transient_linear_gain() const
{
    if(current_db_ == min_db_) {
        return 0;
    } else {
        return DBToLinear(current_db_);
    }
}

double Transient::get_min_db() const { return min_db_; }
double Transient::get_max_db() const { return max_db_; }
double Transient::get_target_db() const { return target_db_.load(); }

void Transient::set_target_db(double db) {
    target_db_.store(hwm::Clamp<double>(db, min_db_, max_db_));
}

NS_HWM_END
