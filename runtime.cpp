#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* ) {}));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (object.TryAs<Number>() && object.TryAs<Number>()->GetValue()){
        return true;
    }
    if (object.TryAs<Bool>() && object.TryAs<Bool>()->GetValue()){
        return true;
    }
    if (object.TryAs<String>() && object.TryAs<String>()->GetValue().length()){
        return true;
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    const Method *method_str = cls_.GetMethod("__str__"s);
    if (method_str){
        ObjectHolder holder = method_str->body.get()->Execute(closure_, context);
        if (holder.Get())
            holder.Get()->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    return cls_.HasMethod(method, argument_count);
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls)
    : cls_(cls) {
    
}

ObjectHolder ClassInstance::Call(const std::string& name, const std::vector<ObjectHolder>& actual_args,
                      Context& context){
    const Method *method;
    if (!HasMethod(name, actual_args.size()) || !(method = cls_.GetMethod(name, actual_args.size())))
        throw std::runtime_error("Method does not exist"s);
    
    Closure closure;
    for (size_t i = 0; i < actual_args.size(); ++i){
        closure[method->formal_params.at(i)] = actual_args.at(i);
    }
    if (!closure.count("self"s))
        closure["self"s] = ObjectHolder::Share(*this);

    return method->body.get()->Execute(closure, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name), parent_(parent) {
    methods_.reserve(methods.size());
    for (Method method : methods){
        methods_.push_back(method);
    }
}

const Method* Class::GetMethod(const std::string& name, size_t args_count) const{      
    const Class* cls = this;

    while (cls){
        for (const Method& method : cls->methods_){
            if (method.name == name){                    
                size_t formal_params_size = method.formal_params.size();                
                if (formal_params_size && method.formal_params.at(0) == "self"s){
                    formal_params_size--;
                }
                if (formal_params_size == args_count){
                    return &method;
                }
            }
        }
        cls = cls->parent_;
    }

    return nullptr;
}

const Method* Class::GetMethod(const std::string& name) const {      
    const Class* cls = this;

    while (cls){
        for (const Method& method : cls->methods_){
            if (method.name == name){
                return &method;
            }
        }
        cls = cls->parent_;
    }

    return nullptr;
}

[[nodiscard]] inline const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]]Context& context) {
    os << "Class "s << GetName();
}

bool Class::HasMethod(const std::string& name, size_t argument_count) const{
    const Class* cls = this;
    while (cls){
        for (const Method& method : cls->methods_){
            if (method.name == name){
                size_t formal_params_size = method.formal_params.size();
                if (formal_params_size && method.formal_params.at(0) == "self"s){
                    formal_params_size--;
                }
                if (formal_params_size == argument_count){
                    return true;
                }
            }
        }
        cls = cls->parent_;
    }

    return false;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context){
    if (!lhs.Get() && !rhs.Get()){
        return true;
    }
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()){
        if (lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()){
        if (lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()){
        if (lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__eq__", 1)){
        return lhs.TryAs<ClassInstance>()->Call("__eq__", {rhs}, context).TryAs<Bool>()->GetValue();
    }
    throw std::runtime_error("Can not compare objects for equality");
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()){
        if (lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()){
        if (lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()){
        if (lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue()){
            return true;
        } else {
            return false;
        }
    }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__lt__", 1)){
        return lhs.TryAs<ClassInstance>()->Call("__lt__", {rhs}, context).TryAs<Bool>()->GetValue();
    }
    throw std::runtime_error("Cannot compare objects for less");
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime