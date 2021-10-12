#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectThrow::ObjectThrow(runtime::ObjectHolder obj)
    : obj_(obj) {

}

runtime::ObjectHolder ObjectThrow::Get(){
    return obj_;
}

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_.get()->Execute(closure, context);
    return closure[var_];
}

Assignment::Assignment(std::string var, std::shared_ptr<Statement> rv)
    : var_(var), rv_(rv) {

}

VariableValue::VariableValue(const std::string& var_name){
    dotted_ids_.push_back(var_name);
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : dotted_ids_(dotted_ids) {

}

ObjectHolder VariableValue::Execute(runtime::Closure& closure, [[maybe_unused]]runtime::Context& context) {
    runtime::Closure *closure_ = &closure;
    runtime::ClassInstance *obj;
    
    for (size_t i = 0; i < dotted_ids_.size() - 1; ++i){
        if (!closure_->count(dotted_ids_.at(i))){
            if (dotted_ids_.at(i) == "self"s){
                continue;
            }
            throw std::runtime_error(dotted_ids_.at(i) + ": unknown variable"s);
        }
        if ((obj = (*closure_)[dotted_ids_.at(i)].TryAs<runtime::ClassInstance>()) == nullptr){
            throw std::runtime_error("Undefined class field"s);
        }
        closure_ = &(obj->Fields());
    }
    if (!closure_->count(dotted_ids_.back())){
        throw std::runtime_error("Unknown variable"s);
    }
    return (*closure_)[dotted_ids_.back()];
}

unique_ptr<Print> Print::Variable(const std::string& name){
        return std::make_unique<Print>(name);
    }

Print::Print(std::unique_ptr<Statement> argument)
    : argument_(std::move(argument)) {
    
}

Print::Print(std::vector<std::unique_ptr<Statement>> args){
    for (size_t i = 0; i < args.size(); ++i){
        args_.push_back(std::move(args.at(i)));
    }
}

ObjectHolder Print::Execute(runtime::Closure& closure, runtime::Context& context) {   
    if (!args_.size()){
        if (closure.count(name_)){            
            closure.at(name_).Get()->Print(context.GetOutputStream(), context);
            context.GetOutputStream() << std::endl;
            return closure.at(name_);
        } else {
            context.GetOutputStream() << "\n";
            return {};
        }
    }
    for (size_t i=0; i<args_.size(); ++i){
        runtime::ObjectHolder obj = args_.at(i).get()->Execute(closure, context);
        if (obj.Get()){
            obj.Get()->Print(context.GetOutputStream(), context);
        } else {
            context.GetOutputStream() << "None";
        }
        if (i != args_.size() - 1){
            context.GetOutputStream() << " ";
        } else {
            context.GetOutputStream() << std::endl;
        }
    }    
    return runtime::ObjectHolder::None();    
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
            std::vector<std::unique_ptr<Statement>> args)
    : object_(std::move(object)), method_(method) {
    args_.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i){
        args_.push_back(std::move(args.at(i)));
    }
}

ObjectHolder MethodCall::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder obj = object_.get()->Execute(closure, context);

    if (runtime::ClassInstance* instance = obj.TryAs<runtime::ClassInstance>(); 
        instance && instance->HasMethod(method_, args_.size())){

        std::vector<runtime::ObjectHolder> args_to_pass;
        args_to_pass.reserve(args_.size());
        for (size_t i = 0; i < args_.size(); ++i){
            args_to_pass.push_back(args_.at(i).get()->Execute(closure, context));
        }

        return instance->Call(method_, args_to_pass, context);
    }

    return {};
}

ObjectHolder Stringify::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder obj = statement_.get()->Execute(closure, context);
    if (obj.Get()){
        std::ostringstream str;
        obj.Get()->Print(str, context);
        return runtime::ObjectHolder::Own(runtime::ValueObject<std::string>(str.str()));
    }
    return runtime::ObjectHolder::Own(runtime::ValueObject<std::string>("None"s));
}

ObjectHolder Add::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Number *left = lhs.TryAs<runtime::Number>(),
        *right = rhs.TryAs<runtime::Number>();
        left && right){
        return runtime::ObjectHolder::Own<runtime::Number>(left->GetValue() + right->GetValue());
    } else if (runtime::String *left = lhs.TryAs<runtime::String>(),
        *right = rhs.TryAs<runtime::String>();
        left && right){
        return runtime::ObjectHolder::Own<runtime::String>(left->GetValue() + right->GetValue());
    } else if (runtime::ClassInstance* instance = lhs.TryAs<runtime::ClassInstance>(); 
        instance && instance->HasMethod(ADD_METHOD, 1)){
        return instance->Call(ADD_METHOD, {rhs}, context);
    }

    throw std::runtime_error("Unable to add objects"s);
}

ObjectHolder Sub::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Number *left = lhs.TryAs<runtime::Number>(),
        *right = rhs.TryAs<runtime::Number>();
        left && right){
        return runtime::ObjectHolder::Own<runtime::Number>(left->GetValue() - right->GetValue());
    } 

    throw std::runtime_error("Unable to subtract numbers"s);
}

ObjectHolder Mult::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Number *left = lhs.TryAs<runtime::Number>(),
        *right = rhs.TryAs<runtime::Number>();
        left && right){
        return runtime::ObjectHolder::Own<runtime::Number>(left->GetValue() * right->GetValue());
    } 

    throw std::runtime_error("Unable to multiplicate numbers"s);
}

ObjectHolder Div::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Number *left = lhs.TryAs<runtime::Number>(),
        *right = rhs.TryAs<runtime::Number>();
        left && right){
        return runtime::ObjectHolder::Own<runtime::Number>(left->GetValue() / right->GetValue());
    } 

    throw std::runtime_error("Unable to divide numbers"s);
}

void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
    args_.push_back(std::move(stmt));
}

ObjectHolder Compound::Execute(runtime::Closure& closure, runtime::Context& context) {
    for (size_t i = 0; i < args_.size(); ++i){
        args_.at(i).get()->Execute(closure, context);
    }
    return {};
}

ObjectHolder Return::Execute(runtime::Closure& closure, runtime::Context& context) {
    throw ObjectThrow(statement_.get()->Execute(closure, context));
}

ClassDefinition::ClassDefinition(runtime::ObjectHolder cls)
    : cls_(cls) {
    
}

ObjectHolder ClassDefinition::Execute(runtime::Closure& closure, [[maybe_unused]] runtime::Context& context) {
    std::string class_name = cls_.TryAs<runtime::Class>()->GetName();
    closure[class_name] = cls_;
    return closure[class_name];
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::shared_ptr<Statement> rv)
:  object_(object), field_name_(field_name), rv_(rv) {

}

ObjectHolder FieldAssignment::Execute(runtime::Closure& closure, runtime::Context& context) {
    if (runtime::ClassInstance* instance = object_.Execute(closure, context).TryAs<runtime::ClassInstance>(); instance){
        runtime::Closure& fields = instance->Fields();
        fields[field_name_] = rv_.get()->Execute(closure, context);
        return fields[field_name_];
    }
    return closure[field_name_];
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body)
    : condition_(std::move(condition)), if_body_(std::move(if_body)),
    else_body_(std::move(else_body)) {

}

ObjectHolder IfElse::Execute(runtime::Closure& closure, runtime::Context& context) {     
    std::ostringstream out;
    condition_.get()->Execute(closure, context).Get()->Print(out, context);

    if (out.str() == "True"s || out.str() == "1"s){
        return if_body_.get()->Execute(closure, context);
    } else if (else_body_.get()){   
        return else_body_.get()->Execute(closure, context);
    } else {
        return {};
    }
}

ObjectHolder Or::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Bool *left = lhs.TryAs<runtime::Bool>(); left && left->GetValue()){         
        return runtime::ObjectHolder::Own<runtime::Bool>(true);
    } else if (runtime::Bool *right = rhs.TryAs<runtime::Bool>(); right && right->GetValue()){      
        return runtime::ObjectHolder::Own<runtime::Bool>(true);
    }
    return runtime::ObjectHolder::Own<runtime::Bool>(false);
}

ObjectHolder And::Execute(runtime::Closure& closure, runtime::Context& context) {
    runtime::ObjectHolder lhs = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder rhs = rhs_.get()->Execute(closure, context);

    if (runtime::Bool *left = lhs.TryAs<runtime::Bool>();
        !left || !left->GetValue()){     
        return runtime::ObjectHolder::Own<runtime::Bool>(false);
    } else if (runtime::Bool *right = rhs.TryAs<runtime::Bool>();
        right && right->GetValue()){
        return runtime::ObjectHolder::Own<runtime::Bool>(true);
    }  
    return runtime::ObjectHolder::Own<runtime::Bool>(false);
}

ObjectHolder Not::Execute(runtime::Closure& closure, runtime::Context& context) {        
    return runtime::ObjectHolder::Own<runtime::Bool>(!(statement_.get()->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()));
}

Comparison::Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) {

}

ObjectHolder Comparison::Execute(runtime::Closure& closure, runtime::Context& context) {
    bool res = cmp_(lhs_.get()->Execute(closure, context), 
        rhs_.get()->Execute(closure, context),
        context);

    return runtime::ObjectHolder::Own(runtime::Bool(res));
}

NewInstance::NewInstance(const runtime::Class& class_){
    obj_ = runtime::ObjectHolder::Own(runtime::ClassInstance(class_));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args){
    obj_ = runtime::ObjectHolder::Own(runtime::ClassInstance(class_));

    args_.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i){
        args_.push_back(std::move(args.at(i)));
    }
}

ObjectHolder NewInstance::Execute([[maybe_unused]] runtime::Closure& closure, [[maybe_unused]] runtime::Context& context) {
    if (runtime::ClassInstance* instance = obj_.TryAs<runtime::ClassInstance>();
        instance && instance->HasMethod(INIT_METHOD, args_.size())){         
        std::vector<runtime::ObjectHolder> args_to_pass;

        args_to_pass.reserve(args_.size());
        for (size_t i = 0; i < args_.size(); ++i){
            args_to_pass.push_back(args_.at(i).get()->Execute(closure, context));
        }

        instance->Call(INIT_METHOD, args_to_pass, context);
    }

    return obj_;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(std::move(body)) {
    
}

ObjectHolder MethodBody::Execute(runtime::Closure& closure, runtime::Context& context) {
    try{
        body_.get()->Execute(closure, context);
        return {};
    } catch (ObjectThrow& err) {
        return err.Get();
    }
}

}  // namespace ast