import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'
import remarkMath from 'remark-math'
import rehypeKatex from 'rehype-katex'
import rehypeHighlight from 'rehype-highlight'
import 'katex/dist/katex.min.css'
import 'highlight.js/styles/github-dark.css'
import mermaid from 'mermaid'
import { useEffect, useRef, useState } from 'react'

// Initialize mermaid once
mermaid.initialize({
    startOnLoad: false,
    theme: 'default',
    securityLevel: 'loose',
})

let mermaidCounter = 0

const MermaidDiagram = ({ content }: { content: string }) => {
    const containerRef = useRef<HTMLDivElement>(null)
    const [svg, setSvg] = useState<string>('')
    const [error, setError] = useState<string | null>(null)
    const idRef = useRef(`mermaid-diagram-${mermaidCounter++}`)

    useEffect(() => {
        const renderDiagram = async () => {
            if (!content) return
            try {
                const { svg: renderedSvg } = await mermaid.render(idRef.current, content)
                setSvg(renderedSvg)
                setError(null)
            } catch (err) {
                console.error('Mermaid render error:', err)
                setError('Failed to render diagram')
            }
        }
        renderDiagram()
    }, [content])

    if (error) {
        return <div className="text-red-500 text-sm p-4 border border-red-300 rounded">{error}</div>
    }

    return (
        <div
            ref={containerRef}
            className="flex justify-center my-6 p-4 bg-white rounded-lg overflow-auto"
            dangerouslySetInnerHTML={{ __html: svg }}
        />
    )
}

interface MarkdownBroadcasterProps {
    content: string
}

export function MarkdownBroadcaster({ content }: MarkdownBroadcasterProps) {
    return (
        <article className="prose prose-lg prose-stone dark:prose-invert max-w-none
                          prose-headings:font-bold prose-headings:tracking-tight
                          prose-h1:text-4xl prose-h1:border-b prose-h1:pb-2 prose-h1:mb-6
                          prose-h2:text-2xl prose-h2:mt-8 prose-h2:mb-4
                          prose-h3:text-xl prose-h3:mt-6
                          prose-p:leading-relaxed prose-p:mb-4
                          prose-code:before:content-none prose-code:after:content-none
                          prose-pre:bg-gray-900 prose-pre:rounded-lg">
            <ReactMarkdown
                remarkPlugins={[remarkGfm, remarkMath]}
                rehypePlugins={[rehypeKatex, rehypeHighlight]}
                components={{
                    code({ className, children, ...props }) {
                        const match = /language-(\w+)/.exec(className || '')
                        const isMermaid = match && match[1] === 'mermaid'
                        const isInline = !className

                        if (isMermaid) {
                            return <MermaidDiagram content={String(children).replace(/\n$/, '')} />
                        }

                        // Block code - let rehype-highlight handle styling
                        if (!isInline && match) {
                            return (
                                <code className={className} {...props}>
                                    {children}
                                </code>
                            )
                        }

                        // Inline code
                        return (
                            <code className="bg-muted px-1.5 py-0.5 rounded text-sm font-mono" {...props}>
                                {children}
                            </code>
                        )
                    }
                }}
            >
                {content}
            </ReactMarkdown>
        </article>
    )
}


