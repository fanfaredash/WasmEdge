package SnapCaller_test

import (
	"SnapCaller"
	"fmt"
	"testing"
)

func TestAll(t *testing.T) {
	SnapCaller.WasmRun("fib_detail.wasm", "", "stage_1.meta")
	SnapCaller.WasmRun("fib_detail.wasm", "stage_1.meta", "stage_2.meta")
	SnapCaller.WasmRun("fib_detail.wasm", "stage_2.meta", "stage_3.meta")

	result, err := SnapCaller.WasmValidate("fib_detail.wasm", "stage_1.meta", "stage_2.meta")
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}

	result, err = SnapCaller.WasmValidate("fib_detail.wasm", "", "stage_1.meta")
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}

	result, err = SnapCaller.WasmValidate("fib_detail.wasm", "stage_2.meta", "")
	if err != nil {
		fmt.Printf("Error: %s\n", err)
	} else {
		fmt.Printf("Result: %t\n", result)
	}
}
