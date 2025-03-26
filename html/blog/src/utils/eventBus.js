// 简单的事件总线实现，用于组件间通信
// 参考 Vue 2 的事件总线模式，但适用于 Vue 3

import { reactive } from 'vue';

class EventBus {
    constructor() {
        this.events = reactive({});
    }

    // 注册事件监听器
    on(event, callback) {
        if (!this.events[event]) {
            this.events[event] = [];
        }
        this.events[event].push(callback);
    }

    // 触发事件
    emit(event, ...args) {
        console.debug(`[EventBus] 触发事件 "${event}"`, args);
        
        if (this.events[event]) {
            console.debug(`[EventBus] 事件 "${event}" 有 ${this.events[event].length} 个监听器`);
            
            // 使用微任务(Promise)而非宏任务(setTimeout)处理，提高优先级
            Promise.resolve().then(() => {
                this.events[event].forEach(callback => {
                    try {
                        callback(...args);
                    } catch (error) {
                        console.error(`[EventBus] 事件 "${event}" 回调执行错误:`, error);
                    }
                });
                console.debug(`[EventBus] 事件 "${event}" 所有回调执行完成`);
            });
        } else {
            console.warn(`[EventBus] 事件 "${event}" 没有监听器`);
        }
    }

    // 移除事件监听器
    off(event, callback) {
        if (this.events[event]) {
            if (callback) {
                this.events[event] = this.events[event].filter(cb => cb !== callback);
            } else {
                this.events[event] = [];
            }
        }
    }
}

// 创建一个全局的事件总线实例
export default new EventBus(); 