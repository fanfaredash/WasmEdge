package main

import (
	"fmt"
	"os"

	"hostfunc/wasmedge"
)

type host struct {
}

// Host function for writting memory
func (h *host) consume(_ interface{}, callframe *wasmedge.CallingFrame, params []interface{}) ([]interface{}, wasmedge.Result) {
	return nil, wasmedge.Result_Success
}

func (h *host) costGetStatus(_ interface{}, callframe *wasmedge.CallingFrame, params []interface{}) ([]interface{}, wasmedge.Result) {
	status := callframe.GetExecutor().GetStatus()
	returns := make([]interface{}, 1)
	returns[0] = status
	return returns, wasmedge.Result_Success
}

func (h *host) costSetStatus(_ interface{}, callframe *wasmedge.CallingFrame, params []interface{}) ([]interface{}, wasmedge.Result) {
	status := params[0].(int32)
	callframe.GetExecutor().SetStatus(status)
	return nil, wasmedge.Result_Success
}

func main() {
	fmt.Println("Go: Args:", os.Args)
	// Expected Args[0]: program name (./externref)
	// Expected Args[1]: wasm file (funcs.wasm)

	// Set not to print debug info
	// wasmedge.SetLogErrorLevel()

	conf := wasmedge.NewConfigure(wasmedge.WASI)
	conf.SetStatisticsCostMeasuring(true)
	vm := wasmedge.NewVMWithConfig(conf)
	obj := wasmedge.NewModule("env")

	h := host{}
	// Add host functions into the module instance

	funcWriteType := wasmedge.NewFunctionType(
		[]wasmedge.ValType{},
		[]wasmedge.ValType{})
	hostconsume := wasmedge.NewFunction(funcWriteType, h.consume, nil, 100)
	obj.AddFunction("consumeGas100", hostconsume)

	funcGetStatusType := wasmedge.NewFunctionType(
		[]wasmedge.ValType{},
		[]wasmedge.ValType{
			wasmedge.ValType_I32,
		})
	hostGetStatus := wasmedge.NewFunction(funcGetStatusType, h.costGetStatus, nil, 0)
	obj.AddFunction("costGetStatus", hostGetStatus)

	funcSetStatusType := wasmedge.NewFunctionType(
		[]wasmedge.ValType{
			wasmedge.ValType_I32,
		},
		[]wasmedge.ValType{})
	hostSetStatus := wasmedge.NewFunction(funcSetStatusType, h.costSetStatus, nil, 0)
	obj.AddFunction("costSetStatus", hostSetStatus)

	vm.RegisterModule(obj)

	vm.LoadWasmFile(os.Args[1])
	vm.Validate()
	vm.Instantiate()

	r, _ := vm.Execute("run", uint32(0))
	fmt.Printf("Cost Status: %d\n", r[0])

	obj.Release()
	vm.Release()
	conf.Release()
}
