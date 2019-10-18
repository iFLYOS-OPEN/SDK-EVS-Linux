#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

extern "C" {

#include <jerryscript-core.h>
#include <iotjs_magic_strings.h>
#include <iotjs_binding.h>
#include <iotjs_def.h>
#include "iotjs_module_buffer.h"

}

#include <memory>
#include <sys/time.h>

#include "iotjs_module_iFLYOS_threadpool.h"

static long currentTime(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    long time_in_mill =
            (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
    return time_in_mill;
}

class Device {
public:
    static std::shared_ptr<Device> create(const std::string& bus, const uint16_t addr, int& error){
        auto fd = open(bus.c_str(), O_RDWR);
        if(fd < 0) {
            printf("open i2c device %s failed. because(%d) %s\n", bus.c_str(), errno, strerror(errno));
            error = errno;
            return nullptr;
        }
        auto ret = ioctl(fd, I2C_SLAVE_FORCE, addr);
        if(ret < 0){
            printf("set I2C_SLAVE_FORCE to i2c device %s failed. because(%d) %s\n", bus.c_str(), errno, strerror(errno));
            close(fd);
        }

        return std::shared_ptr<Device>(new Device(fd, addr));
    }

    bool read(int bytes, uint8_t* to) {
        struct i2c_smbus_ioctl_data args;
        union i2c_smbus_data data;
        args.read_write = I2C_SMBUS_READ;
        args.size = I2C_SMBUS_BYTE_DATA;
        args.data = &data;

        //printf("i2c readwith length %d\n", bytes);
        //only byte mode now
        for(auto i = 0; i < bytes; i++) {
            args.command = i;
            int ret = ioctl(m_fd, I2C_SMBUS, &args);

            if (ret < 0) {
                printf("%s : read i2c bytes returns %d. errno:%d\n", __func__, ret, (int) errno);
                return false;
            }
            to[i] = data.byte;
        }

        return true;
    }

    bool write(std::shared_ptr<std::vector<char>> data){
        struct i2c_msg i2c_msg;
        i2c_msg.len = data->size();
        i2c_msg.addr = m_address;
        i2c_msg.flags = 0;
        i2c_msg.buf = (uint8_t*)data->data();

        struct i2c_rdwr_ioctl_data i2c_data;
        i2c_data.msgs = &i2c_msg;
        i2c_data.nmsgs = 1;

        //printf("i2c write cmd %d with length %d\n", (int)(*data)[0], (int)data->size());
        long ctlStart = currentTime();
        int ret = ioctl(m_fd, I2C_RDWR, (unsigned long)&i2c_data);
        long ctlEnd = currentTime();
        long duration = ctlEnd - ctlStart;
        if(duration > 4) {
            printf("long ioctl/write, [%ld, %ld] %ld\n", ctlStart, ctlEnd, duration);
            printf("addr %x, length: %d, data[0]:%d \n", m_address, data->size(), (int)(*data)[0]);
        }
        if(ret < 0) {
            //printf("%s : write %d bytes returns %d. errno:%d\n", __func__, (int)data->size(), ret, (int) errno);
        }

        return ret >= 0;
    }

    ~Device(){
        close(m_fd);
    }

private:
    Device(const int fd, const uint16_t addr):m_fd{fd}, m_address{addr}{

    }

    const int m_fd;
    const uint16_t m_address;
};

template<typename  T>
struct SPHolder{
    std::shared_ptr<T> p;
};

struct iotjs_i2c_t{
    std::shared_ptr<Device> dev;
    iflyos::ThreadPool worker{1};
};

IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(i2c);

static void iotjs_i2c_destroy(iotjs_i2c_t* cxt){
    cxt->dev.reset();
}

struct CommonAsyncCxt{
    uv_async_t async;
    int error{0};
    jerry_value_t cb{0};
    jerry_value_t jBuffer{0};
};

void asyncDel(uv_handle_t* handle){
    auto cxt = (CommonAsyncCxt*)handle->data;
    delete cxt;
}

void asyncCB(uv_async_t* async){
    auto cxt = (CommonAsyncCxt*)async->data;
    if(cxt->error){
        auto jErr = jerry_get_value_from_error(
                jerry_create_error(JERRY_ERROR_COMMON, (const jerry_char_t *) strerror(cxt->error)),
                true);
        iotjs_invoke_callback(cxt->cb, jerry_create_undefined(), &jErr, 1);
        jerry_release_value(jErr);
    }else{
        iotjs_invoke_callback(cxt->cb, jerry_create_undefined(), nullptr, 0);
    }

    if(cxt->jBuffer){
        jerry_release_value(cxt->jBuffer);
    }
    jerry_release_value(cxt->cb);
    //printf("uv_close\n");
    uv_close((uv_handle_t*)&cxt->async, asyncDel);
}

JS_FUNCTION(Constructor) {
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(3, string, number, function);
    //printf("Constructor()\n");
    auto iotstr = iotjs_jval_as_string(jargv[0]);
    const std::string filePath(iotstr.data, iotstr.size);
    iotjs_string_destroy(&iotstr);
    auto addr = (uint16_t )jerry_get_number_value(jargv[1]);

    auto native = new iotjs_i2c_t;
    native->dev = nullptr;
    auto asyncCxt = new CommonAsyncCxt;
    //printf("async init\n");
    uv_async_init(iotjs_environment_loop(iotjs_environment_get()), &asyncCxt->async, asyncCB);
    asyncCxt->async.data = asyncCxt;
    asyncCxt->cb = jerry_acquire_value(jargv[2]);

    jerry_set_object_native_pointer(jthis, native, &this_module_native_info);
    native->worker.enqueue([native, filePath, addr, asyncCxt]{
        //printf("open device in worker\n");
        native->dev = Device::create(filePath, addr, asyncCxt->error);
        if(native->dev){
            asyncCxt->error = 0;
        }
        //printf("async send\n");
        uv_async_send(&asyncCxt->async);
    });

    return jerry_create_undefined();
}


JS_FUNCTION(Write){
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(1, object);
    DJS_CHECK_ARG_IF_EXIST(1, function);

    JS_DECLARE_THIS_PTR(i2c, native);

    jerry_value_t jcb = 0;
    if(jargc > 1){
        jcb = jerry_acquire_value(jargv[1]);
    }

    auto bufferWrapper = iotjs_bufferwrap_from_jbuffer(jargv[0]);
    auto buf = std::make_shared<std::vector<char>>();
    buf->insert(buf->begin(), &bufferWrapper->buffer[0], &bufferWrapper->buffer[bufferWrapper->length]);

    CommonAsyncCxt* asyncData = nullptr;

    if(jcb) {
        asyncData = new CommonAsyncCxt;
        asyncData->cb = jcb;
        asyncData->async.data = asyncData;
        uv_async_init(iotjs_environment_loop(iotjs_environment_get()), &asyncData->async, asyncCB);
    }
    native->worker.enqueue([native, asyncData, buf, jcb]{
        auto ok = native->dev->write(buf);
        if(jcb) {
            asyncData->error = ok ? 0 : errno;
            uv_async_send(&asyncData->async);
        }
    });
    return jerry_create_undefined();
}

JS_FUNCTION(Read) {
    DJS_CHECK_THIS();
    DJS_CHECK_ARGS(2, object, function);

    JS_DECLARE_THIS_PTR(i2c, native);

    auto asyncCxt = new CommonAsyncCxt;
    uv_async_init(iotjs_environment_loop(iotjs_environment_get()), &asyncCxt->async, asyncCB);
    asyncCxt->async.data = asyncCxt;
    asyncCxt->cb = jerry_acquire_value(jargv[1]);
    asyncCxt->jBuffer = jerry_acquire_value(jargv[0]);

    //the buffer will not be released, because async acquired the Buffer js object
    auto bufferWrapper = iotjs_bufferwrap_from_jbuffer(jargv[0]);
    jerry_set_object_native_pointer(jthis, native, &this_module_native_info);
    native->worker.enqueue([native, bufferWrapper, asyncCxt]{
        //printf("open device in worker\n");
        if(native->dev->read(bufferWrapper->length, (uint8_t *)bufferWrapper->buffer)){
            asyncCxt->error = 0;
        }else{
            asyncCxt->error = errno;
        }

        //printf("async send\n");
        uv_async_send(&asyncCxt->async);
    });

    return jerry_create_undefined();
}

extern "C" jerry_value_t InitiFLYOSI2c() {
    jerry_value_t ji2c_cons = jerry_create_external_function(Constructor);

    jerry_value_t prototype = jerry_create_object();

    iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_WRITE, Write);
    iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_READ, Read);
    iotjs_jval_set_property_jval(ji2c_cons, IOTJS_MAGIC_STRING_PROTOTYPE,
                                 prototype);

    jerry_release_value(prototype);

    return ji2c_cons;
}