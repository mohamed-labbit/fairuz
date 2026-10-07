#include "../fvm.hpp"

namespace fairuz::runtime {

namespace {

Value* dict_field(ObjDict* dict, Value key)
{
    return dict == nullptr ? nullptr : dict->data.find_ptr(key);
}

} // anonymous namespace

Value VM::task_start(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_dict() || !argv[2].is_list())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "task start expects executor, callable, and argument list");
    Value closed_key = m_gc.make_string("closed");
    Value* closed = dict_field(argv[0].as_dict(), closed_key);
    if (closed != nullptr && closed->is_truthy())
        raise_error(ErrorCode::TYPE_ERROR_CALL, "executor is closed");

    Value value = call_value_sync(argv[1], argv[2].as_list());
    Value task = m_gc.make_dict();
    task.as_dict()->set(m_gc.make_string("kind"), m_gc.make_string("task"));
    task.as_dict()->set(m_gc.make_string("done"), Value::from_bool(true));
    task.as_dict()->set(m_gc.make_string("cancelled"), Value::from_bool(false));
    task.as_dict()->set(m_gc.make_string("result"), value);
    return task;
}

Value VM::task_done(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "task done expects a handle");
    Value key = m_gc.make_string("done");
    Value* value = dict_field(argv[0].as_dict(), key);
    return Value::from_bool(value != nullptr && value->is_truthy());
}

Value VM::task_result(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_number())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "task result expects a handle and timeout");
    Value key = m_gc.make_string("result");
    Value* value = dict_field(argv[0].as_dict(), key);
    return value == nullptr ? Value::nil() : *value;
}

Value VM::task_cancel(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "task cancel expects a handle");
    Value done_key = m_gc.make_string("done");
    Value* done = dict_field(argv[0].as_dict(), done_key);
    if (done != nullptr && done->is_truthy())
        return Value::from_bool(false);
    argv[0].as_dict()->set(m_gc.make_string("cancelled"), Value::from_bool(true));
    argv[0].as_dict()->set(done_key, Value::from_bool(true));
    return Value::from_bool(true);
}

Value VM::task_wait_all(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_list() || !argv[1].is_number())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "wait all expects task handles and timeout");
    Value results = m_gc.make_list();
    Value result_key = m_gc.make_string("result");
    for (Value handle : argv[0].as_list()->elements) {
        if (!handle.is_dict())
            raise_error(ErrorCode::NATIVE_TYPE_ERROR, "invalid task handle");
        Value* value = dict_field(handle.as_dict(), result_key);
        results.as_list()->elements.push(value == nullptr ? Value::nil() : *value);
    }
    return results;
}

Value VM::dynamic_call(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[1].is_list())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dynamic call expects a callable and argument list");
    return call_value_sync(argv[0], argv[1].as_list());
}

Value VM::executor_new(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_int() || argv[0].as_int() <= 0)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "executor expects a positive worker count");
    Value result = m_gc.make_dict();
    result.as_dict()->set(m_gc.make_string("kind"), m_gc.make_string("executor"));
    result.as_dict()->set(m_gc.make_string("closed"), Value::from_bool(false));
    return result;
}

Value VM::executor_close(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_bool())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "executor close expects a handle and wait flag");
    argv[0].as_dict()->set(m_gc.make_string("closed"), Value::from_bool(true));
    return Value::from_bool(true);
}

} // fairuz::runtime
