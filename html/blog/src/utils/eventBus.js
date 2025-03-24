// 创建一个简单的事件总线
class EventBus {
  constructor() {
    this.events = {};
  }

  // 订阅事件
  on(eventName, callback) {
    if (!this.events[eventName]) {
      this.events[eventName] = [];
    }
    this.events[eventName].push(callback);
  }

  // 取消订阅
  off(eventName, callback) {
    if (!this.events[eventName]) return;
    
    if (!callback) {
      // 如果没有提供特定回调，则移除所有该事件的监听器
      delete this.events[eventName];
      return;
    }
    
    this.events[eventName] = this.events[eventName].filter(
      cb => cb !== callback
    );
  }

  // 发射事件
  emit(eventName, ...args) {
    if (!this.events[eventName]) return;
    
    this.events[eventName].forEach(callback => {
      callback(...args);
    });
  }
}

// 创建并导出事件总线实例
export default new EventBus(); 