import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'
import 'katex/dist/katex.min.css'
import 'highlight.js/styles/github-dark.css'

interface MarkdownExcerptProps {
    content: string
    maxLength?: number
}

export function MarkdownExcerpt({ content, maxLength = 200 }: MarkdownExcerptProps) {
    // Basic truncation for the raw content before rendering to avoid rendering huge documents
    // This is an approximation, the CSS line-clamp does the visual heavy lifting
    const truncatedContent = content.length > maxLength * 2
        ? content.slice(0, maxLength * 2) + '...'
        : content

    return (
        <div className="prose prose-sm dark:prose-invert max-w-none 
                        prose-p:leading-normal prose-p:m-0 
                        prose-headings:text-base prose-headings:m-0
                        line-clamp-3 text-muted-foreground pointer-events-none">
            <ReactMarkdown
                remarkPlugins={[remarkGfm]}
                allowedElements={['p', 'strong', 'em', 'code', 'span']}
                unwrapDisallowed={true}
                components={{
                    // Override paragraphs to ensure they don't add extra spacing that breaks the excerpt flow
                    p: ({ children }) => <span className="mr-1">{children}</span>
                }}
            >
                {truncatedContent}
            </ReactMarkdown>
        </div>
    )
}
