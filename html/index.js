import "@babel/polyfill";
import main from "/main.svelte";

const warning = document.getElementById("warning");
warning.parentNode.removeChild(warning);

const root = new main({ target: document.body });

export default root;
