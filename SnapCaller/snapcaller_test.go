package SnapCaller_test

import (
	"SnapCaller"
	"testing"
)

/*
func TestAll(t *testing.T) {
	SnapCaller.WasmRun("fib_detail.wasm", "", "stage_1.meta", 2500)
	SnapCaller.WasmRun("fib_detail.wasm", "stage_1.meta", "stage_2.meta", 2500)
	SnapCaller.WasmRun("fib_detail.wasm", "stage_2.meta", "stage_3.meta", 2500)

	result, err := SnapCaller.WasmValidate("fib_detail.wasm", "stage_1.meta", "stage_2.meta", 2500)
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}

	result, err = SnapCaller.WasmValidate("fib_detail.wasm", "stage_1.meta", "stage_2.meta", 1250)
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}

	result, err = SnapCaller.WasmValidate("fib_detail.wasm", "stage_2.meta", "", 2500)
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}
}*/

func TestHttp(t *testing.T) {
	SnapCaller.WasmRun("rustHTTP.wasm", "", "test.meta", 1000000)
}
