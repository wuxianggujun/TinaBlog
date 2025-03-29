// 简单的事件总线实现，用于组件间通信
// 参考 Vue 2 的事件总线模式，但适用于 Vue 3

class EventBus {
    constructor() {
        this._events = {};
    }

    // 注册事件监听器
    on(event, callback) {
        console.debug(`[EventBus] 添加监听器: ${event}`);
        if (!this._events[event]) {
            this._events[event] = [];
        }
        this._events[event].push(callback);
    }

    // 触发事件
    emit(event, ...args) {
        console.debug(`[EventBus] 触发事件: ${event}`);
        
        if (this._events[event]) {
            console.debug(`[EventBus] 事件 "${event}" 有 ${this._events[event].length} 个监听器`);
            
            this._events[event].forEach(callback => {
                try {
                    callback(...args);
                } catch (error) {
                    console.error(`[EventBus] 事件处理器错误:`, error);
                }
            });
        } else {
            console.debug(`[EventBus] 事件 "${event}" 没有监听器`);
        }
    }

    // 移除事件监听器
    off(event, callback) {
        console.debug(`[EventBus] 移除监听器: ${event}`);
        if (this._events[event]) {
            if (callback) {
                this._events[event] = this._events[event].filter(cb => cb !== callback);
            } else {
                this._events[event] = [];
            }
        }
    }
}

// 创建一个事件总线实例
const eventBusInstance = new EventBus();

// 选择要导出的事件总线实例
let exportedEventBus = eventBusInstance;

// 如果window对象存在，处理eventBus
if (typeof window !== 'undefined') {
    if (window.eventBus) {
        // 如果window.eventBus已存在，使用它
        console.debug('[EventBus] 使用已存在的window.eventBus');
        exportedEventBus = window.eventBus;
    } else {
        // 否则，将新创建的实例设置为window.eventBus
        console.debug('[EventBus] 创建新的window.eventBus');
        window.eventBus = eventBusInstance;
    }
} else {
    console.debug('[EventBus] 非浏览器环境');
}

// 导出确定的事件总线实例
export default exportedEventBus; 