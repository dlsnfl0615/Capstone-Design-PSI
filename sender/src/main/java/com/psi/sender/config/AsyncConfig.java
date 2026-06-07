package com.psi.sender.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

import java.util.concurrent.Executor;

@Configuration
@EnableAsync // 비동기 기능 활성화
public class AsyncConfig {
    @Bean(name = "psiExecutor")
    public ThreadPoolTaskExecutor psiExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(1); // 최소 스레드 수를 1개로 제한하여 순차 처리 보장
        executor.setMaxPoolSize(1); // 최대 스레드 수도 1개로 제한
        executor.setQueueCapacity(500); // 동시에 대기할 수 있는 요청의 크기
        executor.setThreadNamePrefix("PsiWorker-");
        executor.initialize();
        return executor;
    }
}
