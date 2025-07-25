#include <iostream>
#include <dlfcn.h>
#include <chrono>
#include "include/model_func.hpp"
#include "yaml-cpp/yaml.h"
/*
在链接动态库时可能会遇到找不到库的问题，解决方法如下：
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${shared_library_abs_directory}

*/

// yolo det
typedef YoloV8_det* (*InitYOLODetModelFunc)(std::string bmodel_file, int dev_id, model_inference_params params, std::vector<std::string> model_class_names);
typedef object_detect_result_list (*InferenceYOLODetModelFunc)(YoloV8_det* model, cv::Mat input_image, bool enable_logger);

// yolo obb
typedef YoloV8_obb* (*InitYOLOObbModelFunc)(std::string bmodel_file, int dev_id, model_inference_params params, std::vector<std::string> model_class_names);
typedef object_obb_result_list (*InferenceYOLOObbModelFunc)(YoloV8_obb* model, cv::Mat input_image, bool enable_logger);

// yolo pose
typedef YoloV8_pose* (*InitYOLOPoseModelFunc)(std::string bmodel_file, int dev_id, model_pose_inference_params params, std::vector<std::string> model_class_names);
typedef object_pose_result_list (*InferenceYOLOPoseModelFunc)(YoloV8_pose* model, cv::Mat input_image, bool enable_logger);

// resnet cls
typedef RESNET* (*InitResNetClsModelFunc)(std::string bmodel_file, int dev_id);
typedef int (*InferenceResNetClsModelFunc)(RESNET* model, cv::Mat input_image, bool enable_logger);
typedef cls_result (*InferenceResNetClsModelRetFunc)(RESNET* model, cv::Mat input_image, bool enable_logger);

// face_attr
typedef RESNET_NC* (*InitMultiClassModelFunc)(std::string bmodel_file, int dev_id);
typedef cls_model_result (*InferenceMultiClassModelFunc)(RESNET_NC* model, cv::Mat input_image, bool enable_logger);

// ppocr
// typedef PPOCR_Detector* (*InitPPOCRDetModelFunc)(std::string bmodel_file, int dev_id);
// typedef PPOCR_Rec* (*InitPPOCRRecModelFunc)(std::string bmodel_file, int dev_id);
// typedef int (*InferencePPOCRDetRecModelFunc)(PPOCR_Detector* ppocr_det, PPOCR_Rec* ppocr_rec);
typedef ocr_result_list (*InferencePPOCRDetRecModelFunc)(std::string bmodel_det, std::string bmodel_rec, cv::Mat input_image, bool enable_logger);
// 加载动态so库
static int loadSo(const char* soPath, void*& handle) {
	handle = dlopen(soPath, RTLD_LAZY);
	if (!handle) {
		std::cerr << "Cannot open library: " << dlerror() << std::endl;
		return 1;
	}
	dlerror();  // 清除之前的错误
	return 0;
}

// 读取yaml文件
static YAML::Node ReadYamlFile(const char* yaml_path) {
	// Load YAML configuration
	YAML::Node config = YAML::LoadFile(yaml_path);
	YAML::Node model_node = config["models"];

	if (!model_node) {
		std::cerr << "Unknown model_name: models " << std::endl;
		throw std::invalid_argument("Unknown model_name: models");
	}
	return model_node;
}

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("%s <model_name> <image_path>\n", argv[0]);
		return -1;
	}
	int ret = 0;
	int dev_id = 0;
	const char* model_name = argv[1];  // det_person
	const char* image_path = argv[2];  // image dirname

	// 打开动态库
	dlerror();
	void* handle = NULL;
	const char* so_path = "libbm_model_library.so";
	ret = loadSo(so_path, handle);
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol :" << dlsym_error << std::endl;
        dlclose(handle);
        return 1;
    }

	// 读取yaml文件
	YAML::Node config = ReadYamlFile("models.yaml");
	YAML::Node model_node = config[model_name];
	if (!model_node) {
		std::cerr << "Unknown model_name: " << model_name << std::endl;
		throw std::invalid_argument("Unknown model_name: " + std::string(model_name));
	}
	const std::string init_func_name = model_node["init_func_name"].as<std::string>();
	const std::string infer_func_name = model_node["function_name"].as<std::string>();
	const std::string bmodel_file = model_node["path"].as<std::string>();
	const std::string enable_log_str = model_node["enable_log"].as<std::string>();
	bool enable_log = (enable_log_str == "true" || enable_log_str == "True");
	const std::string model_type = model_node["model_type"].as<std::string>();

	// 加载图像
	cv::Mat input_image = cv::imread(image_path, cv::IMREAD_COLOR);
	if(input_image.empty()) {
		std::cerr << "Failed to read image: " << image_path << std::endl;
		return 1;
	}

	if (model_type == "yolo_det") { // yolo检测模型
		InitYOLODetModelFunc init_model = (InitYOLODetModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceYOLODetModelFunc inference_model = (InferenceYOLODetModelFunc)dlsym(handle, infer_func_name.c_str());

		model_inference_params params;
		params.input_height = model_node["params"]["input_height"].as<int>();
		params.input_width = model_node["params"]["input_width"].as<int>();
		params.nms_threshold = model_node["params"]["nms_threshold"].as<float>();
		params.box_threshold = model_node["params"]["box_threshold"].as<float>();
		// 读取models.yaml文件的class_names
		std::vector<std::string> class_names;
		for (const auto& class_name : model_node["class_names"]) {
			class_names.push_back(class_name.as<std::string>());
		}
		// initialize net
		YoloV8_det* model = init_model(bmodel_file, dev_id, params, class_names);
		auto start = std::chrono::high_resolution_clock::now();
		object_detect_result_list result = inference_model(model, input_image, enable_log);
		std::cout << "result size: " << result.count << std::endl;
		std::cout << "Success to inference model" << std::endl;
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		std::cout << "yolo模型执行时间: " << duration.count() << " 毫秒" << std::endl;
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}else if (model_type == "res_rec") { // 单分类模型
		std::cout << "Start to inference classification model" << std::endl;
		// 读取models.yaml文件的class_names
		std::vector<std::string> class_names;
		for (const auto& class_name : model_node["class_names"]) {
			class_names.push_back(class_name.as<std::string>());
		}
		InitResNetClsModelFunc init_model = (InitResNetClsModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceResNetClsModelRetFunc inference_model = (InferenceResNetClsModelRetFunc)dlsym(handle, infer_func_name.c_str());
		RESNET* model = init_model(bmodel_file, dev_id);
		std::cout << "Success to init model" << std::endl;
		cls_result ret = inference_model(model, input_image, enable_log);
		std::cout << "Success to inference model" << std::endl;
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}else if (model_type == "multi_class") { // 多分类模型
		std::cout << "Start to inference classification model" << std::endl;
		std::vector<std::string> class_names;
		for (const auto& class_name : model_node["class_names"]) {
			class_names.push_back(class_name.as<std::string>());
		}
		std::vector<std::vector<std::string>> class_values;
		YAML::Node class_values_node = model_node["class_values"];  // Fixed fs -> config
		for (YAML::const_iterator it = class_values_node.begin(); it != class_values_node.end(); ++it) {
			std::vector<std::string> values;
			for (YAML::const_iterator jt = it->begin(); jt != it->end(); ++jt) {
				values.push_back(jt->as<std::string>());
			}
			class_values.push_back(values);
		}
		InitMultiClassModelFunc init_model = (InitMultiClassModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceMultiClassModelFunc inference_model = (InferenceMultiClassModelFunc)dlsym(handle, infer_func_name.c_str());
		RESNET_NC* model = init_model(bmodel_file, dev_id);
		cls_model_result cls_result = inference_model(model, input_image, enable_log);
		std::cout << "模型输出类别数量: " << cls_result.num_class << std::endl;
		for (int i=0; i < cls_result.num_class; i++){
			std::cout << "类别: " << class_names[i] << " 输出: " << class_values[i][cls_result.cls_output[i]] << std::endl;
		}
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}else if (model_type == "ppocr") {
		// std::cout << "init ppocr det model" << std::endl;
		const std::string det_bmodel_file = model_node["det_path"].as<std::string>();
		// const std::string init_det_func = model_node["init_det_func_name"].as<std::string>();
		// InitPPOCRDetModelFunc init_det_model = (InitPPOCRDetModelFunc)dlsym(handle, init_det_func.c_str()); 
		// PPOCR_Detector* det_model = init_det_model(det_bmodel_file, dev_id);
		// std::cout << "init ppocr rec model" << std::endl;
		const std::string rec_bmodel_file = model_node["rec_path"].as<std::string>();
		// const std::string init_rec_func = model_node["init_rec_func_name"].as<std::string>();
		// InitPPOCRRecModelFunc init_rec_model = (InitPPOCRRecModelFunc)dlsym(handle, init_rec_func.c_str()); 
		// PPOCR_Rec* rec_model = init_rec_model(rec_bmodel_file, dev_id);
		// std::cout << "infer ppocr det and rec model" << std::endl;
		InferencePPOCRDetRecModelFunc inference_model = (InferencePPOCRDetRecModelFunc)dlsym(handle, infer_func_name.c_str());
		std::cout << "Success to load model" << std::endl;
		// int ret = inference_model(det_model, rec_model);
		ocr_result_list result = inference_model(det_bmodel_file, rec_bmodel_file, input_image, enable_log);
		std::cout << "Success to inference model" << std::endl;
	}else if (model_type == "yolo_obb") { // yolo旋转框检测模型
		std::cout << "Start to load model" << std::endl;
		InitYOLOObbModelFunc init_model = (InitYOLOObbModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceYOLOObbModelFunc inference_model = (InferenceYOLOObbModelFunc)dlsym(handle, infer_func_name.c_str());

		model_inference_params params;
		params.input_height = model_node["params"]["input_height"].as<int>();
		params.input_width = model_node["params"]["input_width"].as<int>();
		params.nms_threshold = model_node["params"]["nms_threshold"].as<float>();
		params.box_threshold = model_node["params"]["box_threshold"].as<float>();
		// 读取models.yaml文件的class_names
		std::vector<std::string> class_names;
		for (const auto& class_name : model_node["class_names"]) {
			class_names.push_back(class_name.as<std::string>());
			std::cout << class_name.as<std::string>() << std::endl;
		}
		// initialize net
		YoloV8_obb* model = init_model(bmodel_file, dev_id, params, class_names);
		if (!model) {
			std::cerr << "Failed to initialize model" << std::endl;
			return -1;
		}
		std::cout << "init model done" << std::endl;
		if (input_image.empty()) {
			std::cerr << "Empty input image" << std::endl;
			delete model;
			return -1;
		}
		object_obb_result_list result = inference_model(model, input_image, enable_log);
		std::cout << "inference model done" << std::endl;
		std::cout << "result size: " << result.count << std::endl;
		std::cout << "Success to inference model" << std::endl;
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}
	else if (model_type == "yolo_pose") {
    InitYOLOPoseModelFunc init_model = (InitYOLOPoseModelFunc)dlsym(handle, init_func_name.c_str());
    InferenceYOLOPoseModelFunc inference_model = (InferenceYOLOPoseModelFunc)dlsym(handle, infer_func_name.c_str());

    if (!init_model || !inference_model) {
        std::cerr << "dlsym failed: " << dlerror() << std::endl;
        return -1;
    }

    model_pose_inference_params params;
    params.input_height = model_node["params"]["input_height"].as<int>();
    params.input_width = model_node["params"]["input_width"].as<int>();
    params.nms_threshold = model_node["params"]["nms_threshold"].as<float>();
    params.box_threshold = model_node["params"]["box_threshold"].as<float>();
    params.kpt_nums = model_node["params"]["kpt_nums"].as<int>();

    std::vector<std::string> class_names;
    for (const auto& class_name : model_node["class_names"]) {
        class_names.push_back(class_name.as<std::string>());
    }

    YoloV8_pose* model = init_model(bmodel_file, dev_id, params, class_names);
    if (!model) {
        std::cerr << "init_model returned nullptr" << std::endl;
        return -1;
    }

    if (input_image.empty()) {
        std::cerr << "Empty input image" << std::endl;
        delete model;
        return -1;
    }

    object_pose_result_list result = inference_model(model, input_image, enable_log);
    delete model;
	}
	else {
		std::cout << "model_type ERROR !" << std::endl;
	}
	
	if (handle != nullptr) {
		dlclose(handle); 
		handle = nullptr;
	}
	std::cout << "Success to dlclose library" << std::endl;
	return ret;
}