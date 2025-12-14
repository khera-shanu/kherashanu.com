import { useEffect } from "react"
import mermaid from "mermaid"

mermaid.initialize({
    startOnLoad: false,
    theme: "default",
    securityLevel: "loose",
    themeVariables: {
        fontFamily: "inherit",
    },
})

export const useMermaid = (content: string, id: string) => {
    useEffect(() => {
        const render = async () => {
            const element = document.getElementById(id)
            if (element && content) {
                try {
                    const { svg } = await mermaid.render(`mermaid-${id}`, content)
                    element.innerHTML = svg
                } catch (error) {
                    console.error("Mermaid failed to render", error)
                    element.innerHTML = `<p class="text-red-500 text-sm">Validating diagram...</p>`
                }
            }
        }
        render()
    }, [content, id])
}
