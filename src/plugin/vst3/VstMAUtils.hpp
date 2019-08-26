#pragma once

//! @file
/*! VST-MAの仕組みを扱いやすくするためのユーティリティクラスや関数を定義するファイル
 */

#include <cassert>
#include <memory>
#include <utility>

#include <pluginterfaces/base/ftypes.h>
#include <pluginterfaces/base/ipluginbase.h>

#include "../../misc/StrCnv.hpp"
#include "../../misc/Either.hpp"

NS_HWM_BEGIN

//! VST-MAのrelease()メンバ関数を呼び出して、自身の参照カウントを減少させるDeleterクラス
class SelfReleaser
{
public:
    void operator() (Steinberg::FUnknown *p) {
        if(p) {
            p->release();
        }
    }
};

//! VST-MAのオブジェクトを安全に扱うために、SelfReleaserクラスをdeleterに指定したunique_ptr
template<class T>
using vstma_unique_ptr = std::unique_ptr<T, SelfReleaser>;

//! FUnknownから派生したクラスのポインタを受け取り、それをvstma_unique_ptrで管理する
template<class T>
vstma_unique_ptr<T>  to_unique(T *p)
{
    return vstma_unique_ptr<T>(p);
}

template<class To>
using maybe_vstma_unique_ptr = Either<Steinberg::tresult, vstma_unique_ptr<To>>;

template<class To, class T>
maybe_vstma_unique_ptr<To> queryInterface_impl(T *p, Steinberg::FIDString iid)
{
    assert(p);
    
    To *obtained = nullptr;
    Steinberg::tresult const res = p->queryInterface(iid, (void **)&obtained);
    if(res == Steinberg::kResultTrue && obtained) {
        return { to_unique(obtained) };
    } else {
        if(res != Steinberg::kResultTrue) {
            return { res };
        } else {
            return { Steinberg::kNoInterface };
        }
    }
}

//! もしPointerの名前空間内に、別のget_raw_pointerが定義されていても、こちらの関数が必ず使用されるようにする。
namespace prevent_adl {
    
    template<class T>
    auto get_raw_pointer(T *p) -> T * { return p; }
    
    template<class T>
    auto get_raw_pointer(T const &p) -> decltype(p.get()) { return p.get(); }
    
} // prevent_adl

//! pに対してFUnknown::queryInterface()を呼び出し、取得したインターフェースのポインタを返す。
/*! @tparam Pointer 引数に渡すFUnknownから派生したクラスのポインタ型。生ポインタか、get()メンバ関数で生ポインタを返すスマートポインタ型であること。
 *  @tparam To 取得したい目的のインターフェースの型。iid静的メンバ変数を持っていること。
 *  @return
 *      * FUnknown::queryInterface()の呼び出しが正常に完了し、有効なポインタが返ってきた場合は、
 *        それをvstma_unique_ptr<To>に変換し、Eitherのrightの値として返す。
 *      * FUnknown::queryInterface()がkResultTrue以外を返して失敗した場合は、そのエラーコードをEitherのrightの値として返す。
 *      * FUnknown::queryInterface()によって取得されたポインタがnullptrだった場合は、kNoInterfaceをleftに設定して返す。
 */
template<class To, class Pointer>
maybe_vstma_unique_ptr<To> queryInterface(Pointer const &p)
{
    return queryInterface_impl<To>(prevent_adl::get_raw_pointer(p), To::iid);
}

template<class To, class Factory>
maybe_vstma_unique_ptr<To> createInstance_impl(Factory *factory, Steinberg::FUID class_id, Steinberg::FIDString iid)
{
    assert(factory);
    To *obtained = nullptr;
    
    Steinberg::tresult const res = factory->createInstance(class_id, iid, (void **)&obtained);
    if(res == Steinberg::kResultTrue && obtained) {
        return { to_unique(obtained) };
    } else {
        return { res };
    }
}

//! 何らかのファクトリクラスに対してcreateInstance()を呼び出ししてコンポーネントを構築し、
//! それを指定したインターフェースのポインタとして返す。
/*! @tparam FactoryPointer 何らかのファクトリクラスのポインタ型。生ポインタか、get()メンバ関数で生ポインタを返すスマートポインタ型であること。
 *  @tparam To 構築したコンポーネント取得したい目的のインターフェースの型。iid静的メンバ変数を持っていること。
 *  @param class_id 目的のコンポーネントを表すCID
 *  @return
 *      * ファクトリクラスのcreateInstance()の呼び出しが正常に完了し、有効なポインタが返ってきた場合は、
 *        それをvstma_unique_ptr<To>に変換し、Eitherのrightの値として返す。
 *      * FUnknown::queryInterface()がkResultTrue以外を返して失敗した場合は、そのエラーコードをEitherのrightの値として返す。
 */
template<class To, class FactoryPointer>
maybe_vstma_unique_ptr<To> createInstance(FactoryPointer const &factory, Steinberg::FUID class_id)
{
    return createInstance_impl<To>(prevent_adl::get_raw_pointer(factory), class_id, To::iid);
}

NS_HWM_END
