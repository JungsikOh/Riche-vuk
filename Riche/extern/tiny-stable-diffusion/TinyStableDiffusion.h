#pragma once

/*
* Generate 2D Image using Tiny Stable Diffusion.
* You must create 'conda env create -f environment.yaml' about tiny-stable-diffusion and update Pillow lib using 'conda update Pillow'.
* And then set to pythonPath from your anaconda env path.
* See readme of tiny-stable-diffusion and you have to download .pth file 
*/
inline void GenerateImage2D(const std::string& prompt) {
	static std::string pythonPath = R"(C:\Users\story\anaconda3\envs\ldm\python.exe)";											// Set your anaconda env path.
	static std::string scriptPath = "extern\\tiny-stable-diffusion\\tiny_optimizedSD\\tiny_txt2img.py";							// You can stay it still.
	static std::string cmd = pythonPath + " " + scriptPath + " --prompt " + '"' + prompt + '"' + " --H 512 --W 512 --seed 27";	// You can change the number of seed.
	
	std::cout << "[Running cmd] " << cmd << std::endl;
	int retCode = std::system(cmd.c_str());

	if (retCode != 0) {
		std::cerr << "Failed to run Python script. Return code: " << retCode << std::endl;
	}
	else {
		std::cout << "[Image generation done!]" << std::endl;
	}
}