//YDJiang@iflytek.com
#ifndef _IFLYOSINTERFACE_INCLUDE_JS_HELPER_H_
#define _IFLYOSINTERFACE_INCLUDE_JS_HELPER_H_

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
    std::shared_ptr<JHelper> getProperty(const std::string& name) const;
    std::shared_ptr<JHelper> invoke(const std::initializer_list<std::shared_ptr<JHelper>>& paramsList);
    std::shared_ptr<JHelper> invoke(const std::vector<std::shared_ptr<JHelper>>& paramsList);
    std::shared_ptr<JHelper> invoke(const std::vector<std::string>& paramsList);

    bool isError();
    std::string toString();

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
    static std::shared_ptr<JString> create(jerry_value_t t);
    static std::shared_ptr<JString> create(const std::string& v);
    operator const std::string&() const;
    operator const char*() const;
    const std::string& str() const;
private:
    JString(jerry_value_t v, const std::string& str);
    const std::string mValueStr;
};

#endif //_IFLYOSINTERFACE_INCLUDE_JS_HELPER_H_