extern "C" {
	fn consumeGas100();
	fn costGetStatus() -> i32;
	fn costSetStatus(status: i32);
}

#[no_mangle]
pub unsafe extern fn run(flag: i32) -> i32 {
	consumeGas100();
	consumeGas100();
	if flag == 0 {
		costSetStatus(1);
	}
	consumeGas100();
	return costGetStatus() as i32;
}



