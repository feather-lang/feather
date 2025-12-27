<script setup>
defineProps({
  href: String,
  logo: String,
  logoAlt: String,
  title: String,
  status: String,
  clickable: {
    type: Boolean,
    default: false
  }
})
</script>

<template>
  <component
    :is="clickable ? 'a' : 'div'"
    :href="clickable ? href : undefined"
    class="platform-card"
    :class="{ 'platform-card-link': clickable }"
  >
    <div class="platform-logo-col">
      <img :src="logo" :alt="logoAlt" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3 :data-status="status">{{ title }}</h3>
      <slot />
      <span v-if="clickable" class="platform-cta">Get started →</span>
    </div>
  </component>
</template>

<style scoped>
.platform-card {
  flex: 1 1 calc(33% - 24px);
  max-width: calc(33% - 12px);
  min-width: 280px;
  padding: 24px;
  border-radius: 12px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  display: flex;
  gap: 20px;
  align-items: flex-start;
}

@media (max-width: 1024px) {
  .platform-card {
    flex: 1 1 calc(50% - 24px);
    max-width: calc(50% - 12px);
  }
}

@media (max-width: 640px) {
  .platform-card {
    flex: 1 1 100%;
    max-width: 100%;
  }
}

.platform-logo-col {
  flex-shrink: 0;
  width: 80px;
  display: flex;
  justify-content: center;
}

.platform-logo {
  height: 56px;
  width: auto;
}

.platform-text {
  text-align: left;
}

.platform-text h3 {
  margin: 0 0 8px 0;
  font-size: 1.2em;
  display: flex;
  align-items: center;
  gap: 8px;
}

.platform-text h3::after {
  content: attr(data-status);
  font-size: 0.65em;
  font-weight: 600;
  text-transform: uppercase;
  padding: 2px 6px;
  border-radius: 4px;
  background: var(--vp-c-gray-2);
  color: var(--vp-c-text-2);
}

.platform-text h3[data-status="alpha"]::after {
  background: var(--vp-c-green-soft);
  color: var(--vp-c-green-1);
}

.platform-text h3[data-status="planned"]::after {
  background: var(--vp-c-yellow-soft);
  color: var(--vp-c-yellow-1);
}

.platform-text h3[data-status="unsupported"]::after {
  background: var(--vp-c-gray-soft);
  color: var(--vp-c-text-3);
}

.platform-text :deep(p) {
  margin: 0;
  color: var(--vp-c-text-2);
  font-size: 0.95em;
}

.platform-card-link {
  text-decoration: none;
  cursor: pointer;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.platform-card-link:hover {
  border-color: var(--vp-c-brand-1);
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
}

.platform-cta {
  display: inline-block;
  margin-top: 12px;
  color: var(--vp-c-brand-1);
  font-weight: 500;
  font-size: 0.9em;
}
</style>
