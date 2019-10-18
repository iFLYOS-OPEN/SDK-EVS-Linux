//
// Created by jan on 19-5-16.
//

#ifndef MESSAGER_IOTJS_BINDING_CPP_HELPER_H
#define MESSAGER_IOTJS_BINDING_CPP_HELPER_H


#include <memory>
#include <vector>
#include <list>
#include <string>
#include <jerryscript.h>

class JHelper {
public:
#ifndef NDEBUG
    static std::list<int> objs;
#endif
    static jerry_value_t unmanagedError(jerry_error_t t, const std::string& msg);
    static jerry_value_t unmanagedUndefined();
    static std::shared_ptr<JHelper> undefined();
    //wrapper the JS object created or get in native
    static std::shared_ptr<JHelper> create(jerry_value_t t);
    //create a wrapper with a JS object function argument
    static std::shared_ptr<JHelper> acquire(jerry_value_t t);
    static std::shared_ptr<JHelper> promise();

    //forget it in native, reference will pass to VM
    operator const jerry_value_t() const;
    virtual ~JHelper();
    void setProperty(jerry_value_t name, jerry_value_t value);
    void setProperty(const std::string& name, jerry_value_t value);
    void setProperty(const std::string& name, jerry_external_handler_t fun);
    void getStringProperty(const std::string& name, std::string& value) const;
    std::shared_ptr<JHelper> getProperty(const std::string& name) const;
    std::shared_ptr<JHelper> invoke(const std::initializer_list<std::shared_ptr<JHelper>>& paramsList);
    std::shared_ptr<JHelper> invoke(const std::vector<std::shared_ptr<std::string>>& paramsList);
    std::shared_ptr<JHelper> callMethod(const std::string& methodName, const std::vector<std::shared_ptr<std::string>>& paramsList);
    std::shared_ptr<JHelper> callMethod(const std::string& methodName, std::vector<std::shared_ptr<JHelper>> paramsList);
    std::shared_ptr<JHelper> invoke(const std::vector<std::shared_ptr<JHelper>>& paramsList);
    std::shared_ptr<JHelper> invoke(const std::vector<std::string>& paramsList);

    bool isError();

private:
#ifndef NDEBUG
    int myObjId;
    static int steadyObjId;
#endif

protected:
    JHelper(jerry_value_t v);
    const jerry_value_t mValue;
};

class JString : public JHelper{
public:
    static std::shared_ptr<JString> acquire(jerry_value_t t);
    static std::shared_ptr<JString> create(const std::string& v);
    operator const std::string&() const;
    operator const char*() const;
    const std::string& str() const;
private:
    JString(jerry_value_t v, const std::string& str);
    const std::string mValueStr;
};

#endif //MESSAGER_IOTJS_BINDING_CPP_HELPER_H
