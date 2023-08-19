package SnapCaller

import (
	"errors"
	"math/rand"
	"os"
	"os/exec"
)

// 路径按实际情况修改
var wasmedgeDir string = "/home/njx/WasmEdge/"
var wasmedgePath string = "build/tools/wasmedge/wasmedge"

// var argSetGasLimit string = "--gas-limit"
var argEnableSnapshot string = "--enable-snapshot"
var argSnapshotInput string = "--snapshot-input"
var argSnapshotOutput string = "--snapshot-output"
var tempPath string = "/tmp/"

const (
	ExitSuccess           int = 0
	ExitCostLimitExceeded int = 134
	ExitFailed            int = -1
)

const letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

func createRandomFile() string {
	var err error = errors.New("")
	var file *os.File
	var filePath string
	for err != nil {
		b := make([]byte, 32)
		for i := range b {
			b[i] = letters[rand.Intn(len(letters))]
		}
		filePath = tempPath + string(b) + ".tmp"
		file, err = os.OpenFile(filePath, os.O_RDWR|os.O_CREATE, 0755)
	}
	file.Close()
	return filePath
}

func removeRandomFile(filePath string) {
	os.Remove(filePath)
}

func execCore(cmd *exec.Cmd) (int, error) {
	if err := cmd.Start(); err != nil {
		return ExitFailed, err
	}

	if err := cmd.Wait(); err != nil {
		if exiterr, ok := err.(*exec.ExitError); ok {
			return exiterr.ExitCode(), nil
		} else {
			return ExitFailed, err
		}
	}

	return ExitSuccess, nil
}

func wasmStart(wasmPath, outputPath string) (int, error) {
	execPath := wasmedgeDir + wasmedgePath
	cmd := exec.Command(execPath,
		argEnableSnapshot,
		argSnapshotOutput,
		outputPath,
		wasmPath,
	)
	return execCore(cmd)
}

func wasmContinue(wasmPath, inputPath, outputPath string) (int, error) {
	execPath := wasmedgeDir + wasmedgePath
	cmd := exec.Command(execPath,
		argEnableSnapshot,
		argSnapshotInput,
		inputPath,
		argSnapshotOutput,
		outputPath,
		wasmPath,
	)
	return execCore(cmd)
}

/* Function: 		WasmRun
 * Description: 	Run wasm program with snapshot input.
 * Arguments: 		wasmPath, inputPath, outputPath.
 * Return value:	int, error
 *
 * Explaination: 	如果是首次运行程序，则参数 inputPath 为空串；
 *					如果 gas 充足，返回 ExitSuccess，同时 outputPath 不产生新的快照。
 *					反之，返回 ExitCostLimitExceeded，同时 outputPath 产生新的快照。
 */
func WasmRun(wasmPath, inputPath, outputPath string) (int, error) {
	if inputPath == "" {
		return wasmStart(wasmPath, outputPath)
	} else {
		return wasmContinue(wasmPath, inputPath, outputPath)
	}
}

/* Function: 		WasmValidate
 * Description: 	Verify snapshot correctness.
 * Arguments: 		wasmPath, snapshotPath, nextSnapshotPath.
 * Return value:	bool, error
 *
 * Explaination: 	如果希望验证程序的首个快照，则参数 snapshotPath 为空串；
 *					如果希望验证程序的最后一个快照，则参数 nextSnapshotPath 为空串。
 */
func WasmValidate(wasmPath, snapshotPath, nextSnapshotPath string) (bool, error) {
	tempFilePath := createRandomFile()
	defer removeRandomFile(tempFilePath)

	if exitcode, err := WasmRun(wasmPath, snapshotPath, tempFilePath); err == nil {
		switch exitcode {
		case ExitSuccess:
			return nextSnapshotPath == "", nil
		case ExitCostLimitExceeded:
			cmd := exec.Command("diff", nextSnapshotPath, tempFilePath)
			result, err := execCore(cmd)
			if err != nil {
				return false, err
			} else {
				return result == 0, nil
			}
		default:
			return false, errors.New("wasm exitcode error")
		}
	} else {
		return false, err
	}
}
