// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "StrictModules/Objects/string_object.h"

#include "StrictModules/Objects/object_interface.h"
#include "StrictModules/Objects/objects.h"

#include "StrictModules/Objects/callable_wrapper.h"

#include <sstream>

namespace strictmod::objects {
// StrictType
StrictString::StrictString(
    std::shared_ptr<StrictType> type,
    std::shared_ptr<StrictModuleObject> creator,
    std::string value)
    : StrictString(
          std::move(type),
          std::weak_ptr(std::move(creator)),
          std::move(value)) {}

StrictString::StrictString(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    std::string value)
    : StrictInstance(std::move(type), std::move(creator)),
      pyStr_(nullptr),
      value_(value) {}

StrictString::StrictString(
    std::shared_ptr<StrictType> type,
    std::weak_ptr<StrictModuleObject> creator,
    PyObject* pyValue)
    : StrictInstance(std::move(type), std::move(creator)),
      pyStr_(Ref<>(pyValue)),
      value_(PyUnicode_AsUTF8(pyValue)) {}

bool StrictString::isHashable() const {
  return true;
}

size_t StrictString::hash() const {
  return std::hash<std::string>{}(value_);
}

bool StrictString::eq(const BaseStrictObject& other) const {
  try {
    const StrictString& otherStr = dynamic_cast<const StrictString&>(other);
    return value_ == otherStr.value_;
  } catch (const std::bad_cast&) {
    return false;
  }
}

Ref<> StrictString::getPyObject() const {
  if (pyStr_ == nullptr) {
    pyStr_ = Ref<>::steal(PyUnicode_FromString(value_.c_str()));
  }
  return Ref<>(pyStr_.get());
}

std::string StrictString::getDisplayName() const {
  return value_;
}

std::shared_ptr<BaseStrictObject> StrictString::strFromPyObj(
    Ref<> pyObj,
    const CallerContext& caller) {
  return std::make_shared<StrictString>(StrType(), caller.caller, pyObj.get());
}

std::shared_ptr<BaseStrictObject> StrictString::listFromPyStrList(
    Ref<> pyObj,
    const CallerContext& caller) {
  if (!PyList_CheckExact(pyObj.get())) {
    caller.raiseTypeError("str.split did not return a list");
  }
  std::size_t size = PyList_GET_SIZE(pyObj.get());
  std::vector<std::shared_ptr<BaseStrictObject>> data;
  data.reserve(size);
  for (std::size_t i = 0; i < size; ++i) {
    PyObject* elem = PyList_GET_ITEM(pyObj.get(), i);
    auto elemStr =
        std::make_shared<StrictString>(StrType(), caller.caller, elem);
    data.push_back(std::move(elemStr));
  }
  return std::make_shared<StrictList>(
      ListType(), caller.caller, std::move(data));
}

std::shared_ptr<BaseStrictObject> StrictString::str__new__(
    std::shared_ptr<StrictString>,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> instType,
    std::shared_ptr<BaseStrictObject> object) {
  std::shared_ptr<StrictStringType> strType =
      std::dynamic_pointer_cast<StrictStringType>(instType);
  if (strType == nullptr) {
    caller.raiseExceptionStr(
        TypeErrorType(), "X is not a str type object ({})", instType);
  }
  if (object == nullptr) {
    return std::make_shared<StrictString>(
        std::move(strType), caller.caller, "");
  }
  std::string funcName = kDunderStr;
  auto func = iLoadAttrOnType(object, kDunderStr, nullptr, caller);
  if (func == nullptr) {
    funcName = kDunderRepr;
    func = iLoadAttrOnType(object, kDunderRepr, nullptr, caller);
  }
  if (func != nullptr) {
    auto result = iCall(std::move(func), kEmptyArgs, kEmptyArgNames, caller);
    auto resultStr = std::dynamic_pointer_cast<StrictString>(result);
    if (resultStr == nullptr) {
      caller.raiseTypeError(
          "{}.{} must return string, not {}",
          object->getTypeRef().getName(),
          std::move(funcName),
          result->getTypeRef().getName());
    }
    if (strType == StrType()) {
      return resultStr;
    }
    return std::make_shared<StrictString>(
        std::move(strType), caller.caller, resultStr->getValue());
  } else {
    caller.error<UnsupportedException>("str()", object->getDisplayName());
    return makeUnknown(caller, "str({})", object);
  }
}

std::shared_ptr<BaseStrictObject> StrictString::str__len__(
    std::shared_ptr<StrictString> self,
    const CallerContext& caller) {
  return caller.makeInt(self->value_.size());
}

std::shared_ptr<BaseStrictObject> StrictString::str__eq__(
    std::shared_ptr<StrictString> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> other) {
  auto otherStr = std::dynamic_pointer_cast<StrictString>(other);
  if (!otherStr) {
    return StrictFalse();
  }
  return caller.makeBool(self->value_ == otherStr->value_);
}

std::shared_ptr<BaseStrictObject> StrictString::strJoin(
    std::shared_ptr<StrictString> self,
    const CallerContext& caller,
    std::shared_ptr<BaseStrictObject> iterable) {
  std::vector<std::shared_ptr<BaseStrictObject>> elements =
      iGetElementsVec(std::move(iterable), caller);
  std::stringstream ss;
  std::size_t size = elements.size();
  const std::string& sep = self->getValue();
  for (std::size_t i = 0; i < size; ++i) {
    auto elemStr = std::dynamic_pointer_cast<StrictString>(elements[i]);
    if (elemStr == nullptr) {
      caller.raiseTypeError(
          "expect str for element {} of join, got {}",
          i,
          elements[i]->getTypeRef().getName());
    }
    ss << elemStr->getValue();
    if (i != size - 1) {
      ss << sep;
    }
  }
  return caller.makeStr(ss.str());
}

std::shared_ptr<BaseStrictObject> StrictString::str__str__(
    std::shared_ptr<StrictString> self,
    const CallerContext& caller) {
  return caller.makeStr(self->getValue());
}

std::shared_ptr<BaseStrictObject> StrictString::str__iter__(
    std::shared_ptr<StrictString> self,
    const CallerContext& caller) {
  const std::string& value = self->getValue();
  std::vector<std::shared_ptr<BaseStrictObject>> chars;
  chars.resize(value.size());
  for (char c : value) {
    chars.push_back(caller.makeStr(std::string{c}));
  }
  auto list = std::make_shared<StrictTuple>(
      TupleType(), caller.caller, std::move(chars));
  return std::make_shared<StrictSequenceIterator>(
      SequenceIteratorType(), caller.caller, std::move(list));
}

// StrictStringType
std::unique_ptr<BaseStrictObject> StrictStringType::constructInstance(
    std::weak_ptr<StrictModuleObject> caller) {
  return std::make_unique<StrictString>(
      std::static_pointer_cast<StrictType>(shared_from_this()),
      std::move(caller),
      "");
}

std::shared_ptr<StrictType> StrictStringType::recreate(
    std::string name,
    std::weak_ptr<StrictModuleObject> caller,
    std::vector<std::shared_ptr<BaseStrictObject>> bases,
    std::shared_ptr<DictType> members,
    std::shared_ptr<StrictType> metatype,
    bool isImmutable) {
  return createType<StrictStringType>(
      std::move(name),
      std::move(caller),
      std::move(bases),
      std::move(members),
      std::move(metatype),
      isImmutable);
}

std::vector<std::type_index> StrictStringType::getBaseTypeinfos() const {
  std::vector<std::type_index> baseVec = StrictObjectType::getBaseTypeinfos();
  baseVec.emplace_back(typeid(StrictStringType));
  return baseVec;
}

Ref<> StrictStringType::getPyObject() const {
  return Ref<>(reinterpret_cast<PyObject*>(&PyUnicode_Type));
}

void StrictStringType::addMethods() {
  addStaticMethodDefault("__new__", StrictString::str__new__, nullptr);
  addMethod(kDunderLen, StrictString::str__len__);
  addMethod(kDunderStr, StrictString::str__str__);
  addMethod(kDunderIter, StrictString::str__iter__);
  addMethod("__eq__", StrictString::str__eq__);
  addMethod("join", StrictString::strJoin);
  PyObject* strType = reinterpret_cast<PyObject*>(&PyUnicode_Type);
  addPyWrappedMethodObj<1>("__format__", strType, StrictString::strFromPyObj);
  addPyWrappedMethodObj<>(kDunderRepr, strType, StrictString::strFromPyObj);
  addPyWrappedMethodObj<1>("__mod__", strType, StrictString::strFromPyObj);
  addPyWrappedMethodObj<>("isidentifier", strType, StrictBool::boolFromPyObj);
  addPyWrappedMethodObj<>("lower", strType, StrictString::strFromPyObj);
  addPyWrappedMethodObj<>("upper", strType, StrictString::strFromPyObj);

  addPyWrappedMethodObj<1>(
      "__ne__", strType, StrictBool::boolOrNotImplementedFromPyObj);
  addPyWrappedMethodObj<1>(
      "__ge__", strType, StrictBool::boolOrNotImplementedFromPyObj);
  addPyWrappedMethodObj<1>(
      "__gt__", strType, StrictBool::boolOrNotImplementedFromPyObj);
  addPyWrappedMethodObj<1>(
      "__le__", strType, StrictBool::boolOrNotImplementedFromPyObj);
  addPyWrappedMethodObj<1>(
      "__lt__", strType, StrictBool::boolOrNotImplementedFromPyObj);

  addPyWrappedMethodDefaultObj(
      "strip", strType, StrictString::strFromPyObj, 1, 1);
  addPyWrappedMethodDefaultObj(
      "replace", strType, StrictString::strFromPyObj, 1, 3);
  addPyWrappedMethodDefaultObj(
      "startswith", strType, StrictString::strFromPyObj, 2, 3);
  addPyWrappedMethodDefaultObj(
      "split", strType, StrictString::listFromPyStrList, 2, 2);
}
} // namespace strictmod::objects
