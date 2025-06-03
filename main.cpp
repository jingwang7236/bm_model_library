#include <iostream>
#include <dlfcn.h>
#include "include/model_func.hpp"

#include "yaml-cpp/yaml.h"
/*
在链接动态库时可能会遇到找不到库的问题，解决方法如下：
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${shared_library_abs_directory}

*/

// yolo det
typedef YoloV8_det* (*InitYOLODetModelFunc)(std::string bmodel_file, int dev_id, model_inference_params params, std::vector<std::string> model_class_names);
typedef object_detect_result_list (*InferenceYOLODetModelFunc)(YoloV8_det* model, cv::Mat input_image, bool enable_logger);

// resnet cls
typedef RESNET* (*InitResNetClsModelFunc)(std::string bmodel_file, int dev_id);
typedef int (*InferenceResNetClsModelFunc)(RESNET* model, cv::Mat input_image, bool enable_logger);

// face_attr
typedef RESNET_NC* (*InitFaceAttrModelFunc)(std::string bmodel_file, int dev_id);
typedef cls_model_result (*InferenceFaceAttrModelFunc)(RESNET_NC* model, cv::Mat input_image, bool enable_logger);

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

	// 读取yaml文件
	YAML::Node config = YAML::LoadFile("models.yaml");
	YAML::Node env_settings_node = config["env_settings"];

	// 打开动态库
	dlerror();
	void* handle = NULL;
	const char* so_path = env_settings_node["so_path"].as<std::string>();
	ret = loadSo(so_path, handle);
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol :" << dlsym_error << std::endl;
        dlclose(handle);
        return 1;
    }

	YAML::Node model_node = config["models"][model_name];
	if (!model_node) {
		std::cerr << "Unknown model_name: " << model_name << std::endl;
        dlclose(handle);
		return 2;
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
        dlclose(handle);
		return 3;
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
		object_detect_result_list result = inference_model(model, input_image, enable_log);
		std::cout << "result size: " << result.count << std::endl;
		std::cout << "Success to inference model" << std::endl;
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}else if (model_type == "res_rec") { // 分类模型
		std::cout << "Start to inference classification model" << std::endl;
		InitResNetClsModelFunc init_model = (InitResNetClsModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceResNetClsModelFunc inference_model = (InferenceResNetClsModelFunc)dlsym(handle, infer_func_name.c_str());
		RESNET* model = init_model(bmodel_file, dev_id);
		std::cout << "Success to init model" << std::endl;
		int ret = inference_model(model, input_image, enable_log);
		std::cout << "Success to inference model" << std::endl;
		delete model;
		std::cout << "Success to destroy model" << std::endl;
	}else if (model_type == "face_attr") { // 多分类模型
		// std::cout << "Start to inference header det model" << std::endl;
		// InitYOLODetModelFunc init_det_model = (InitYOLODetModelFunc)dlsym(handle, "init_yolov8_det_model");
		// InferenceYOLODetModelFunc inference_det_model = (InferenceYOLODetModelFunc)dlsym(handle, "inference_yolo_person_det_model");
		// const std::string det_bmodel_file = "models/det_header_1684x_f16.bmodel";
		// model_inference_params params;
		// params.input_height = 640;
		// params.input_width = 640;
		// params.nms_threshold = 0.6;
		// params.box_threshold = 0.5;
		// std::vector<std::string> class_names;
		// class_names.push_back("header");
		// YoloV8_det* det_model = init_det_model(det_bmodel_file, dev_id, params, class_names);
		// object_detect_result_list det_result = inference_det_model(det_model, input_image, enable_log);
		// std::cout << "det result size: " << det_result.count << std::endl;
		std::cout << "Start to inference classification model" << std::endl;
		InitFaceAttrModelFunc init_model = (InitFaceAttrModelFunc)dlsym(handle, init_func_name.c_str());
		InferenceFaceAttrModelFunc inference_model = (InferenceFaceAttrModelFunc)dlsym(handle, infer_func_name.c_str());
		RESNET_NC* model = init_model(bmodel_file, dev_id);
		// std::cout << "Success to inference face attr model" << std::endl;
		// for (int i=0;i<det_result.count;i++){
		// 	int cls_id = det_result.results[i].cls_id;
		// 	float score = det_result.results[i].prop;
		// 	int left = det_result.results[i].box.left;
		// 	int top = det_result.results[i].box.top;
		// 	int right = det_result.results[i].box.right;
		// 	int bottom = det_result.results[i].box.bottom;
		// 	cv::Mat img_crop = input_image(cv::Rect(left, top, right - left, bottom - top));
		// 	cls_model_result cls_result = inference_model(model, img_crop, enable_log);
		// 	std::cout << "模型输出类别: " << cls_result.num_class << std::endl;
		// 	for (int i=0; i < cls_result.num_class; i++){
		// 		std::cout << "类别: " << i << " 输出: " << cls_result.cls_output[i] << std::endl;
		// 	}
		// }
		// delete det_model;
		cls_model_result cls_result = inference_model(model, input_image, enable_log);
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