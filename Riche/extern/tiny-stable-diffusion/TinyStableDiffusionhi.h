#pragma once

static void GenerateImage(const std::string& prompt) {
	//std::string pythonPath = R"(C:\Users\story\anaconda3\envs\ldm\python.exe)";
	//std::string scriptPath = "tiny-stable-diffusion-main\\tiny_optimizedSD\\tiny_txt2img.py";
	//std::string scriptPath = "..\\extern\\tiny-stable-diffusion\\tiny_optimizedSD\\tiny_txt2img.py";
	std::string pythonPath = R"(conda run -n ldm python)";
	std::string scriptPath = "..\\extern\\tiny-stable-diffusion\\tiny_optimizedSD\\tiny_txt2img.py";

	std::string cmd = pythonPath + " " + scriptPath + " --prompt " + '"' + prompt + '"' + " --H 512 --W 512 --seed 27";
	std::cout << "Running cmd: " << cmd << std::endl;
	int retCode = std::system(cmd.c_str());

	if (retCode != 0) {
		std::cerr << "Failed to run Python script. Return code: " << retCode << std::endl;
	}
	else {
		std::cout << "Image generation done!" << std::endl;
	}
}