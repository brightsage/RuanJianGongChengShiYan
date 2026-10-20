package com.example.demo;

import org.mybatis.spring.annotation.MapperScan;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.transaction.annotation.EnableTransactionManagement;

/**
 * 高校学生成长与职业发展数据中台 —— 启动类。
 *
 * <p>分层：controller -> service -> mapper -> MySQL，pojo 贯穿各层，
 * 与 tlias 教学项目保持同一套分层约定（Result / PageResult 统一返回）。
 */
@SpringBootApplication
@MapperScan("com.example.demo.mapper")
@EnableTransactionManagement
public class Demo1Application {

    public static void main(String[] args) {
        SpringApplication.run(Demo1Application.class, args);
    }

}
