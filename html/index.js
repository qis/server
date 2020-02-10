import "@babel/polyfill";
import App from "./src/App.svelte";

{
  const warning = document.getElementById("warning");
  warning.parentNode.removeChild(warning);
}

const app = new App({ target: document.body });

export default app;
