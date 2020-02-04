import "@babel/polyfill";
import main from "/src/main.svelte";

const root = new main({ target: document.body });

export default root;
