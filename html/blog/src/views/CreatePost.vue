<template>
  <div class="create-post-container">
    <!-- Cherry Markdown编辑器 -->
    <div id="markdown" class="cherry-editor"></div>

    <!-- 发布对话框 -->
    <div class="publish-dialog" v-if="showDialog" @click.self="cancelPublish">
      <div class="dialog-content" @click.stop>
        <div class="dialog-header">
          <h3>发布文章</h3>
          <button type="button" class="close-btn" @click="cancelPublish">&times;</button>
        </div>

        <div class="dialog-body">
          <div class="form-group">
            <label for="title">文章标题 *</label>
            <input
                type="text"
                id="title"
                v-model="postForm.title"
                placeholder="请输入文章标题"
                required
            />
          </div>

          <div class="form-group">
            <label for="summary">文章摘要</label>
            <textarea
                id="summary"
                v-model="postForm.summary"
                placeholder="请输入文章摘要（可选）"
                rows="3"
            ></textarea>
          </div>

          <div class="form-group">
            <label for="slug">文章链接(slug)</label>
            <input
                type="text"
                id="slug"
                v-model="postForm.slug"
                placeholder="自定义文章链接，不填将自动生成"
            />
            <small class="form-hint">链接格式如：my-first-post（仅使用字母、数字和连字符）</small>
          </div>

          <div class="form-group">
            <label for="categories">分类</label>
            <div class="tag-input-container">
              <input
                  type="text"
                  id="categories"
                  v-model="categoryInput"
                  @keyup.enter.prevent="addCategory"
                  placeholder="输入分类名称并按回车添加"
              />
              <button type="button" class="add-tag-btn" @click="addCategory">添加</button>
              <div class="tags-container">
                <span
                    v-for="(category, index) in postForm.categories"
                    :key="'cat-'+index"
                    class="tag"
                >
                  {{ category }}
                  <button type="button" @click="removeCategory(index)" class="tag-remove">&times;</button>
                </span>
              </div>
            </div>
          </div>

          <div class="form-group">
            <label for="tags">标签</label>
            <div class="tag-input-container">
              <input
                  type="text"
                  id="tags"
                  v-model="tagInput"
                  @keyup.enter.prevent="addTag"
                  placeholder="输入标签名称并按回车添加"
              />
              <button type="button" class="add-tag-btn" @click="addTag">添加</button>
              <div class="tags-container">
                <span
                    v-for="(tag, index) in postForm.tags"
                    :key="'tag-'+index"
                    class="tag"
                >
                  {{ tag }}
                  <button type="button" @click="removeTag(index)" class="tag-remove">&times;</button>
                </span>
              </div>
            </div>
          </div>

          <div class="form-group publish-option">
            <h4>发布设置</h4>
            <div class="publish-options-container">
              <div class="publish-option-toggle">
                <div class="toggle-option" 
                     :class="{ active: postForm.published }"
                     @click="postForm.published = true">
                  <div class="toggle-icon">
                    <svg viewBox="0 0 24 24" width="20" height="20">
                      <path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z" fill="currentColor"/>
                    </svg>
                  </div>
                  <div class="toggle-text">
                    <div class="toggle-title">公开发布</div>
                    <div class="toggle-desc">所有访问者均可查看此文章</div>
                  </div>
                </div>
                
                <div class="toggle-option" 
                     :class="{ active: !postForm.published }"
                     @click="postForm.published = false">
                  <div class="toggle-icon">
                    <svg viewBox="0 0 24 24" width="20" height="20">
                      <path d="M12 7c2.76 0 5 2.24 5 5 0 .65-.13 1.26-.36 1.83l2.92 2.92c1.51-1.26 2.7-2.89 3.43-4.75-1.73-4.39-6-7.5-11-7.5-1.4 0-2.74.25-3.98.7l2.16 2.16C10.74 7.13 11.35 7 12 7zM2 4.27l2.28 2.28.46.46C3.08 8.3 1.78 10.02 1 12c1.73 4.39 6 7.5 11 7.5 1.55 0 3.03-.3 4.38-.84l.42.42L19.73 22 21 20.73 3.27 3 2 4.27zM7.53 9.8l1.55 1.55c-.05.21-.08.43-.08.65 0 1.66 1.34 3 3 3 .22 0 .44-.03.65-.08l1.55 1.55c-.67.33-1.41.53-2.2.53-2.76 0-5-2.24-5-5 0-.79.2-1.53.53-2.2zm4.31-.78l3.15 3.15.02-.16c0-1.66-1.34-3-3-3l-.17.01z" fill="currentColor"/>
                    </svg>
                  </div>
                  <div class="toggle-text">
                    <div class="toggle-title">私人文章</div>
                    <div class="toggle-desc">仅自己可以查看</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <div class="dialog-footer">
          <button type="button" class="cancel-btn" @click="cancelPublish">取消</button>
          <button type="button" class="submit-btn" @click="submitPost" :disabled="isSubmitting">
            {{ isSubmitting ? '提交中...' : '提交文章' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import Cherry from 'cherry-markdown';
import 'cherry-markdown/dist/cherry-markdown.css';
import eventBus from '../utils/eventBus';

export default {
  name: 'CreatePost',
  data() {
    return {
      postForm: {
        title: '',
        content: '',
        summary: '',
        slug: '',
        categories: [],
        tags: [],
        published: true
      },
      categoryInput: '',
      tagInput: '',
      isSubmitting: false,
      showDialog: false,
      editor: null,
    };
  },
  mounted() {
    // 初始化Cherry Markdown编辑器
    this.initEditor();

    // 组件挂载后，验证登录状态
    const token = localStorage.getItem('token');
    if (token) {
      setTimeout(() => {
        this.verifyToken(token);
      }, 500);
    }

    // 检查登录状态
    this.checkLoginStatus();

    // 添加监听器
    window.addEventListener('storage', this.handleStorageChange);

    // 监听发布文章事件
    eventBus.on('openPublishDialog', this.showPublishDialog);
  },
  beforeUnmount() {
    // 移除监听器
    window.removeEventListener('storage', this.handleStorageChange);

    // 移除事件总线监听
    eventBus.off('openPublishDialog', this.showPublishDialog);

    // 销毁编辑器实例
    if (this.editor) {
      this.editor.destroy();
    }
  },
  methods: {
    // 加载外部依赖库
    loadExternalLibraries() {
      // 加载Mermaid库
      if (!document.getElementById('mermaid-script')) {
        const mermaidScript = document.createElement('script');
        mermaidScript.id = 'mermaid-script';
        mermaidScript.src = 'https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js';
        document.head.appendChild(mermaidScript);
      }

      // 加载KaTeX库
      if (!document.getElementById('katex-script')) {
        const katexCSS = document.createElement('link');
        katexCSS.id = 'katex-css';
        katexCSS.rel = 'stylesheet';
        katexCSS.href = 'https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.css';
        document.head.appendChild(katexCSS);

        const katexScript = document.createElement('script');
        katexScript.id = 'katex-script';
        katexScript.src = 'https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.js';
        document.head.appendChild(katexScript);
      }

      // 加载ECharts库
      if (!document.getElementById('echarts-script')) {
        const echartsScript = document.createElement('script');
        echartsScript.id = 'echarts-script';
        echartsScript.src = 'https://cdn.jsdelivr.net/npm/echarts@5.4.3/dist/echarts.min.js';
        document.head.appendChild(echartsScript);

        // 确保ECharts完全加载
        return new Promise((resolve) => {
          echartsScript.onload = () => {
            console.log('ECharts库已加载完成');
            resolve();
          };
        });
      }

      return Promise.resolve();
    },

    // 初始化Cherry Markdown编辑器
    async initEditor() {
      try {
        console.log('开始初始化编辑器...');

        // 加载必要外部库（可选）- 等待加载完成
        await this.loadExternalLibraries();

        // 确保容器元素存在
        const editorContainer = document.getElementById('markdown');
        if (!editorContainer) {
          console.error('找不到编辑器容器元素，ID: markdown');
          setTimeout(() => this.initEditor(), 500); // 延迟重试
          return;
        }

        // 使用新的配置格式
        const config = {
          id: 'markdown',
          value: '# 欢迎使用Tina Blog编辑器\n\n开始撰写你的精彩文章吧！',
          editor: {
            theme: 'default',
            height: '100%',
            defaultModel: 'edit&preview',
            toolbar: [
              'bold',
              'italic',
              'strikethrough',
              '|',
              {header: []},     // 修改：使用对象格式
              {list: []},       // 修改：使用对象格式
              'ordered-list',
              'quote',
              '|',
              'code',
              'code-block',
              '|',
              {table: []},      // 修改：使用对象格式
              'link',
              'image',
              '|',
              'preview',
              'fullscreen'
            ]
          },
          callback: {
            onChange: (markdown) => {
              console.log('编辑器内容已更新，长度:', markdown?.length || 0);
              this.postForm.content = markdown;
            },
            afterInit: () => {
              console.log('编辑器初始化完成');
              // 确保内容被设置
              if (this.editor) {
                this.postForm.content = this.editor.getMarkdown();
              }
            }
          }
        };

        // 初始化编辑器
        console.log('创建Cherry实例...');
        this.editor = new Cherry(config);
        console.log('Cherry Markdown编辑器已初始化:', !!this.editor);

        // 全局暴露编辑器实例，便于调试
        window.cherryEditor = this.editor;

      } catch (error) {
        console.error('初始化编辑器失败:', error);
        alert('编辑器初始化失败，请刷新页面重试');
      }
    },

    // 验证和处理slug
    validateSlug() {
      if (!this.postForm.slug) return; // 如果为空，后端会自动生成

      // 转为小写
      this.postForm.slug = this.postForm.slug.toLowerCase();

      // 替换空格为连字符
      this.postForm.slug = this.postForm.slug.replace(/\s+/g, '-');

      // 移除非法字符（只保留字母、数字和连字符）
      this.postForm.slug = this.postForm.slug.replace(/[^a-z0-9-]/g, '');

      // 确保没有连续的连字符
      this.postForm.slug = this.postForm.slug.replace(/-+/g, '-');

      // 去除首尾连字符
      this.postForm.slug = this.postForm.slug.replace(/^-+|-+$/g, '');
      
      // 确保符合后端验证规则：^[a-z0-9]+(?:-[a-z0-9]+)*$
      const slugRegex = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
      if (!slugRegex.test(this.postForm.slug)) {
        console.warn('Slug不符合格式要求，系统将自动调整');
        // 如果不符合规则，简化处理成只有字母和数字
        this.postForm.slug = this.postForm.slug.replace(/-/g, '');
        
        // 如果调整后仍为空，生成一个基于时间戳的slug
        if (!this.postForm.slug) {
          this.postForm.slug = 'post-' + Date.now().toString().slice(-8);
        }
      }
      
      console.log('验证后的slug:', this.postForm.slug);
    },

    // 显示发布对话框
    showPublishDialog() {
      console.log('CreatePost组件的showPublishDialog方法被调用');
      // 获取编辑器内容
      if (this.editor) {
        this.postForm.content = this.editor.getMarkdown();
        console.log('已获取编辑器内容，长度:', this.postForm.content?.length || 0);
      } else {
        console.warn('编辑器实例不存在');
      }

      if (!this.postForm.content || !this.postForm.content.trim()) {
        console.warn('文章内容为空');
        alert('请先编辑文章内容');
        return;
      }
      console.log('设置showDialog为true');
      this.showDialog = true;
      console.log('showDialog当前值:', this.showDialog);
    },

    // 取消发布
    cancelPublish() {
      this.showDialog = false;
    },

    // 检查登录状态
    checkLoginStatus() {
      const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
      const token = localStorage.getItem('token');

      if (!isLoggedIn || !token) {
        console.warn('未登录状态下访问创建文章页面');
        this.$router.push({
          path: '/login',
          query: {redirect: '/create'}
        });
      } else {
        // 检查token有效性
        this.verifyToken(token);
      }
    },

    handleStorageChange(event) {
      if (event.key === 'isLoggedIn' || event.key === 'token') {
        this.checkLoginStatus();
      }
    },

    addCategory() {
      if (this.categoryInput.trim() && !this.postForm.categories.includes(this.categoryInput.trim())) {
        this.postForm.categories.push(this.categoryInput.trim());
      }
      this.categoryInput = '';
    },

    removeCategory(index) {
      this.postForm.categories.splice(index, 1);
    },

    addTag() {
      if (this.tagInput.trim() && !this.postForm.tags.includes(this.tagInput.trim())) {
        this.postForm.tags.push(this.tagInput.trim());
      }
      this.tagInput = '';
    },

    removeTag(index) {
      this.postForm.tags.splice(index, 1);
    },

    async submitPost() {
      console.log('提交文章表单，开始处理...');
      
      // 确保获取最新的编辑器内容
      if (this.editor) {
        this.postForm.content = this.editor.getMarkdown();
        console.log('已获取编辑器内容，长度:', this.postForm.content.length);
      } else {
        console.warn('编辑器实例不存在');
      }

      if (!this.postForm.title || !this.postForm.content) {
        console.warn('标题或内容为空，终止提交');
        alert('请填写必填字段');
        return;
      }

      // 验证和处理slug
      this.validateSlug();
      
      // 详细检查分类和标签
      console.log('===== 表单数据诊断 =====');
      console.log('标题:', this.postForm.title);
      console.log('摘要:', this.postForm.summary);
      console.log('Slug:', this.postForm.slug);
      console.log('分类数组:', this.postForm.categories);
      console.log('分类数组类型:', Array.isArray(this.postForm.categories) ? 'Array' : typeof this.postForm.categories);
      console.log('分类数组长度:', this.postForm.categories.length);
      console.log('分类JSON:', JSON.stringify(this.postForm.categories));
      console.log('标签数组:', this.postForm.tags);
      console.log('标签数组类型:', Array.isArray(this.postForm.tags) ? 'Array' : typeof this.postForm.tags);
      console.log('标签数组长度:', this.postForm.tags.length);
      console.log('标签JSON:', JSON.stringify(this.postForm.tags));
      console.log('发布状态:', this.postForm.published);
      console.log('========================');

      // 确保分类和标签为数组类型
      if (!Array.isArray(this.postForm.categories)) {
        console.warn('分类不是数组，将自动修正');
        this.postForm.categories = [];
      }
      
      if (!Array.isArray(this.postForm.tags)) {
        console.warn('标签不是数组，将自动修正');
        this.postForm.tags = [];
      }

      try {
        this.isSubmitting = true;
        console.log('设置isSubmitting=true');

        // 从localStorage获取token
        const token = localStorage.getItem('token');
        if (!token) {
          alert('您需要登录后才能发布文章');
          this.$router.push('/login');
          return;
        }

        // 验证token格式
        if (token.trim() === '') {
          alert('登录凭证无效，请重新登录');
          this.handleTokenError();
          return;
        }

        // 先验证token有效性
        try {
          const verifyResponse = await fetch('/api/auth/verify', {
            method: 'GET',
            headers: {
              'Authorization': `Bearer ${token}`
            }
          });

          if (verifyResponse.status === 401) {
            alert('登录已过期，请重新登录');
            this.handleTokenError();
            return;
          }
        } catch (verifyError) {
          console.warn('验证token时出错，继续尝试发布:', verifyError);
          // 验证出错不阻止发布尝试
        }

        // 准备提交数据
        const postData = {
          title: this.postForm.title,
          content: this.postForm.content,
          summary: this.postForm.summary,
          slug: this.postForm.slug,
          categories: this.postForm.categories,
          tags: this.postForm.tags,
          published: this.postForm.published
        };
        
        console.log('准备提交的文章数据:', postData);

        // 创建文章
        const response = await fetch('/api/posts', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
          },
          body: JSON.stringify(postData),
          // 添加请求超时控制
          signal: AbortSignal.timeout(15000) // 15秒超时
        }).catch(error => {
          console.error('请求失败:', error);
          if(error.name === 'AbortError') {
            alert('请求超时，请稍后重试');
          } else {
            alert('网络请求失败，请检查网络连接');
          }
          throw error;
        });

        if (!response) {
          this.isSubmitting = false;
          return;
        }

        // 添加超时后的代码
        let data;
        try {
          data = await response.json();
        } catch (parseError) {
          console.error('解析响应JSON失败:', parseError);
          alert('服务器响应格式错误');
          this.isSubmitting = false;
          return;
        }

        if (response.ok && data.status === 'success') {
          // 创建成功
          this.showDialog = false;

          // 构建文章链接
          const postLink = data.data.slug ? `/post/${data.data.slug}` : `/post/${data.data.id}`;

          // 显示成功信息包含链接
          const message = `文章创建成功！文章ID: ${data.data.id}`;
          alert(message);

          // 跳转到文章详情页或首页
          this.$router.push('/');
        } else {
          // 检查是否是token问题
          if (response.status === 401) {
            alert('登录已过期，请重新登录');
            this.handleTokenError();
          } else {
            // 其他创建失败
            alert(data.message || '创建文章失败，请稍后重试');
          }
        }
      } catch (error) {
        console.error('创建文章出错：', error);
        alert('创建文章失败，请稍后重试');
      } finally {
        this.isSubmitting = false;
      }
    },

    // 处理token错误
    handleTokenError() {
      // 清除无效的登录状态
      localStorage.removeItem('token');
      localStorage.removeItem('username');
      localStorage.removeItem('isLoggedIn');

      // 跳转到登录页
      this.$router.push({
        path: '/login',
        query: {redirect: '/create'}
      });
    },

    async verifyToken(token) {
      try {
        const response = await fetch('/api/auth/verify', {
          method: 'GET',
          headers: {
            'Authorization': `Bearer ${token}`
          }
        });

        if (response.status === 401) {
          console.warn('Token已失效');
          this.handleTokenError();
        }
      } catch (error) {
        console.error('验证token时出错:', error);
        // 网络错误不处理
      }
    }
  }
}
</script>

<style scoped>
.create-post-container {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: calc(100vh - 60px); /* 减去顶部导航栏的高度 */
  overflow: hidden;
  position: relative;
  z-index: 10;
}

.cherry-editor {
  flex: 1;
  width: 100%;
  height: calc(100vh - 60px); /* 减去顶部导航栏的高度 */
}

/* 发布对话框样式 */
.publish-dialog {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 9999; /* 使用非常高的z-index确保显示在最上层 */
}

.dialog-content {
  width: 90%;
  max-width: 600px;
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  overflow: hidden;
  max-height: 90vh;
  display: flex;
  flex-direction: column;
}

.dialog-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 1.5rem;
  background-color: #f9fafb;
  border-bottom: 1px solid #e5e7eb;
}

.dialog-header h3 {
  margin: 0;
  font-size: 1.25rem;
  color: #1f2937;
}

.close-btn {
  background: none;
  border: none;
  font-size: 1.5rem;
  color: #6b7280;
  cursor: pointer;
  padding: 0;
  margin: 0;
  line-height: 1;
}

.dialog-body {
  padding: 1.5rem;
  overflow-y: auto;
  max-height: calc(90vh - 130px);
}

.dialog-footer {
  display: flex;
  justify-content: flex-end;
  gap: 1rem;
  padding: 1rem 1.5rem;
  background-color: #f9fafb;
  border-top: 1px solid #e5e7eb;
}

.form-group {
  margin-bottom: 1.5rem;
}

.form-group label {
  display: block;
  margin-bottom: 0.5rem;
  font-weight: 600;
  color: #374151;
}

.form-group input[type="text"],
.form-group textarea {
  width: 100%;
  padding: 0.75rem;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  font-size: 1rem;
  transition: border-color 0.3s;
  font-family: inherit;
}

.form-group input[type="text"]:focus,
.form-group textarea:focus {
  outline: none;
  border-color: #6366f1;
  box-shadow: 0 0 0 2px rgba(99, 102, 241, 0.2);
}

.form-hint {
  display: block;
  margin-top: 0.25rem;
  font-size: 0.85rem;
  color: #6b7280;
}

.tag-input-container {
  margin-top: 0.5rem;
  display: flex;
  flex-wrap: wrap;
  align-items: center;
}

.tag-input-container input {
  flex: 1;
  min-width: 200px;
}

.tags-container {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-top: 0.75rem;
  width: 100%;
}

.tag {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  background: #f0f1ff;
  border: 1px solid #d2d3fc;
  color: #4f46e5;
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  font-size: 0.9rem;
}

.tag-remove {
  background: none;
  border: none;
  color: #4f46e5;
  cursor: pointer;
  font-size: 1rem;
  line-height: 1;
  padding: 0;
  margin-left: 0.25rem;
}

.publish-option {
  margin-top: 1rem;
}

.publish-option h4 {
  margin-bottom: 0.75rem;
  color: #374151;
  font-size: 1rem;
}

.publish-options-container {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.publish-option-toggle {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.toggle-option {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  user-select: none;
  color: #374151;
  font-weight: normal;
  padding: 0.75rem 1rem;
  border-radius: 0.375rem;
  transition: all 0.2s;
  border: 1px solid #e5e7eb;
}

.toggle-option:hover {
  background-color: #f9fafb;
}

.toggle-icon {
  width: 24px;
  height: 24px;
  flex-shrink: 0;
  color: #9ca3af;
  display: flex;
  align-items: center;
  justify-content: center;
}

.toggle-text {
  display: flex;
  flex-direction: column;
}

.toggle-title {
  font-weight: 500;
  color: #374151;
  font-size: 0.95rem;
}

.toggle-desc {
  font-size: 0.85rem;
  color: #6b7280;
  margin-top: 0.25rem;
}

.toggle-option.active {
  background-color: #f0f5ff;
  border-color: #6366f1;
}

.toggle-option.active .toggle-icon {
  color: #6366f1;
}

.toggle-option.active .toggle-title {
  color: #4f46e5;
}

.cancel-btn, .submit-btn {
  padding: 0.75rem 1.5rem;
  border-radius: 4px;
  font-size: 1rem;
  cursor: pointer;
  transition: all 0.3s;
}

.cancel-btn {
  background: white;
  border: 1px solid #d1d5db;
  color: #4b5563;
}

.cancel-btn:hover {
  background: #f9fafb;
}

.submit-btn {
  background: #6366f1;
  border: none;
  color: white;
}

.submit-btn:hover {
  background: #4f46e5;
}

.submit-btn:disabled {
  background: #a5a6f6;
  cursor: not-allowed;
}

/* 优化编辑器样式 */
:deep(.cherry-markdown pre.cherry-code-block) {
  position: relative;
  overflow-x: auto !important;
}

:deep(.cherry-markdown pre.cherry-code-block.show-line-number) {
  padding-left: 3.5em !important;
}

:deep(.cherry-markdown pre.cherry-code-block code) {
  white-space: pre !important;
  word-wrap: normal !important;
}

:deep(.cherry-markdown .mermaid) {
  background: #f8f8f8;
  padding: 10px;
  border-radius: 4px;
  text-align: center;
}

:deep(.cherry) {
  height: 100% !important;
  max-width: none !important;
}

:deep(.cherry-editor__container) {
  height: 100% !important;
}

.editor-toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 1rem;
  height: 60px;
  background-color: white;
  border-bottom: 1px solid #e5e7eb;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.editor-title {
  font-size: 1.25rem;
  font-weight: 600;
  color: #1f2937;
  margin: 0;
}

.editor-toolbar .publish-btn {
  display: flex;
  align-items: center;
  background-color: #6366f1;
  color: white;
  border: none;
  border-radius: 4px;
  padding: 0.5rem 1rem;
  font-weight: 500;
  cursor: pointer;
  transition: background-color 0.2s;
}

.editor-toolbar .publish-btn:hover {
  background-color: #4f46e5;
}

.add-tag-btn {
  background-color: #6366f1;
  color: white;
  border: none;
  border-radius: 4px;
  padding: 0.5rem 1rem;
  margin-left: 0.5rem;
  cursor: pointer;
  font-size: 0.9rem;
  transition: background-color 0.2s;
}

.add-tag-btn:hover {
  background-color: #4f46e5;
}
</style> 