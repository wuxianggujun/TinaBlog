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
        if (this.events[event]) {
            this.events[event].forEach(callback => {
                callback(...args);
            });
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