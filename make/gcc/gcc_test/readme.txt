注意： 因为发布内核版本而需要编译TEST目录，应该先编译一次lib库文件，并升级一次小版本号。
Commands Discription:
1 编译basic功能：
  A 修改参数debugPortCfg 为 BASIC_MODE,位置在[ custom\config\custom_sys_cfg.c ]
  B 打开宏__OCPU_FOTA_BY_HTTP__ & __OCPU_FOTA_BY_FTP__ ，位置在[SDK_n_LIB\custom\config\custom_feature_def.h]
  C 编译M66项目，需要在fota_stress_http.c中修改为 #define HTTP_UPDATE_FILENAME    "M66_Fotaftp_app.bin"，
    在fota_stress_ftp.c中修改为 #define FTP_FILENAME    "M66_Fotahttp_app.bin"
  D 编译M26项目，需要在fota_stress_http.c中修改为 #define HTTP_UPDATE_FILENAME    "M26_Fotaftp_app.bin"，
    在fota_stress_ftp.c中修改为 #define FTP_FILENAME    "M26_Fotahttp_app.bin"
  E make2 test_basic
  F 继续使用quecFota工具OpenCPU_FOTA_Package_Tool制作相应的升级包

2 编译advance功能：
  A 修改参数debugPortCfg 为 ADVANCE_MODE,位置在[ custom\config\custom_sys_cfg.c ]
  B 打开宏__OCPU_FOTA_BY_HTTP__ & __OCPU_FOTA_BY_FTP__ ，位置在[SDK_n_LIB\custom\config\custom_feature_def.h]
  C 编译M66项目，需要在fota_stress_http.c中修改为 #define HTTP_UPDATE_FILENAME    "M66_Fotaftp_app.bin"，
    在fota_stress_ftp.c中修改为 #define FTP_FILENAME    "M66_Fotahttp_app.bin"
  D 编译M26项目，需要在fota_stress_http.c中修改为 #define HTTP_UPDATE_FILENAME    "M26_Fotaftp_app.bin"，
    在fota_stress_ftp.c中修改为 #define FTP_FILENAME    "M26_Fotahttp_app.bin"
  E make2 test_advance
  F 继续使用quecFota工具OpenCPU_FOTA_Package_Tool制作相应的升级包

3 编译LIB
  make2 newlib

4 Clean
  make2 clean
